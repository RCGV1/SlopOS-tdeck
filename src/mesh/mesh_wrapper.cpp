// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// MeshCore protocol integration using SlopMesh (minimal Mesh subclass).
// MeshCore is MIT licensed (meshcore-dev/MeshCore).

#include "mesh_wrapper.h"
#include "hal/tdeck_board.h"
#include "hal/tdeck_pins.h"
#include "hal/gps.h"
#include "hal/prefs.h"
#include "slop_mesh.h"
#include "../diagnostics/debug_cfg.h"
#include "../meshtastic/meshtastic_node.h"

#include <SPIFFS.h>
#include <Preferences.h>
#include <time.h>
#include <cstring>
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/ESP32Board.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>

using slopos::mesh::MeshMessage;

extern "C" uint32_t lv_timer_handler(void);

// ════════════════════════════════════════════════════
// Global objects
// ════════════════════════════════════════════════════

static slopos::TDeckBoard        board;
static SPIClass                  lora_spi;
static Module*                   lora_mod = new Module(P_LORA_NSS, P_LORA_DIO_1,
                                                       P_LORA_RESET, P_LORA_BUSY, lora_spi);
static CustomSX1262              radio_module(lora_mod);
static CustomSX1262Wrapper       radio_driver(radio_module, board);
static ESP32RTCClock             fallback_clock;
static AutoDiscoverRTCClock      rtc_clock(fallback_clock);
static StdRNG                    fast_rng;
static SimpleMeshTables          tables;
static ArduinoMillis             millis_clock;
static StaticPoolPacketManager   pkt_mgr(16);
static slopos::mesh::SlopMesh*   g_mesh = nullptr;
static slopos::meshtastic::MeshtasticNode g_meshtastic;

static bool initialized = false;
static slopos::ProtocolMode active_protocol = slopos::ProtocolMode::MeshCore;
static char own_name[32] = "SlopOS";
static uint32_t last_advert_time = 0;
static bool     last_advert_success = false;
static bool     last_advert_used_gps = false;
static slopos::meshtastic::FrequencyPlan meshtastic_plan = {};
static uint32_t meshtastic_packet_id = 0;
static bool     meshtastic_tx_pending = false;
static bool     meshtastic_position_after_nodeinfo = false;

static constexpr uint8_t MESHTASTIC_SYNC_WORD = 0x2b;

// ════════════════════════════════════════════════════
// Message queue
// ════════════════════════════════════════════════════

static constexpr int MAX_QUEUED = 64;
static MeshMessage   msg_buf[MAX_QUEUED];
static int           msg_head = 0, msg_tail = 0, msg_count = 0;

static void queue_push(const char* sender, const char* channel, const char* text) {
    if (msg_count >= MAX_QUEUED) return;
    MeshMessage& m = msg_buf[msg_head];
    strncpy(m.sender, sender, sizeof(m.sender) - 1);
    m.sender[sizeof(m.sender) - 1] = '\0';
    strncpy(m.channel, channel ? channel : "", sizeof(m.channel) - 1);
    m.channel[sizeof(m.channel) - 1] = '\0';
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = '\0';
    m.timestamp = rtc_clock.getCurrentTime();
    m.is_self = false;
    msg_head = (msg_head + 1) % MAX_QUEUED;
    msg_count++;
    // Log as packet entry (accessible via Packets screen)
    if (sender && sender[0] && (g_mesh || active_protocol == slopos::ProtocolMode::Meshtastic)) {
        int rssi = (int)radio_driver.getLastRSSI();
        float snr = radio_driver.getLastSNR();
        const char* ptype = (channel && channel[0]) ? "CHANNEL" : "DM";
        slopos::mesh::pushPacketLog(sender, rssi, snr, ptype);
    }
}

static bool queue_pop(MeshMessage* out) {
    if (msg_count == 0) return false;
    *out = msg_buf[msg_tail];
    msg_tail = (msg_tail + 1) % MAX_QUEUED;
    msg_count--;
    return true;
}

static void onMeshMessage(const char* sender, const char* channel, const char* text) {
    queue_push(sender, channel, text);
#if SLOPOS_DEBUG_MESH
    SLOPOS_RUNTIME_FEAT(mesh) {
    int rssi = (int)radio_driver.getLastRSSI();
    float snr = radio_driver.getLastSNR();
    Serial.printf("[mesh] MSG from %s%s%s: %s  (RSSI:%ddBm SNR:%.1fdB)\n",
                  sender, channel && channel[0] ? " in " : "",
                  channel && channel[0] ? channel : "", text, rssi, snr);
    }
#endif
}

// ════════════════════════════════════════════════════
// Identity persistence
// ════════════════════════════════════════════════════

static bool loadIdentity(::mesh::LocalIdentity& id) {
    if (!SPIFFS.exists("/mesh_id")) return false;
    File f = SPIFFS.open("/mesh_id", "r");
    if (!f) return false;
    uint8_t buf[128];
    int len = f.read(buf, sizeof(buf));
    f.close();
    if (len != PRV_KEY_SIZE && len != (PRV_KEY_SIZE + PUB_KEY_SIZE)) {
        // Corrupt or partial file — delete it and regenerate
        SPIFFS.remove("/mesh_id");
        return false;
    }
    id.readFrom(buf, len);
    // validatePrivateKey expects raw 64-byte prv_key — MeshCore serializes prv_key first
    return ::mesh::LocalIdentity::validatePrivateKey(buf);
}

static void saveIdentity(::mesh::LocalIdentity& id) {
    uint8_t buf[128];
    size_t len = id.writeTo(buf, sizeof(buf));
    File f = SPIFFS.open("/mesh_id", "w");
    if (!f) return;
    size_t written = f.write(buf, len);
    f.close();
    if (written != len) {
        // Partial write — file may be corrupted, remove it
        SPIFFS.remove("/mesh_id");
    }
}

static uint32_t nextMeshtasticPacketId() {
    meshtastic_packet_id++;
    if (meshtastic_packet_id == 0) meshtastic_packet_id = 1;
    return meshtastic_packet_id;
}

static uint32_t makeMeshtasticNodeNum(const slopos::NodePrefs& p) {
#if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    uint32_t node = static_cast<uint32_t>(mac & 0xFFFFFFFFu);
    if (node > 3 && node != slopos::meshtastic::kBroadcastNode) return node;
#endif
    uint32_t hash = slopos::meshtastic::djb2Hash(p.node_name);
    if (hash <= 3 || hash == slopos::meshtastic::kBroadcastNode) hash = 0x13572468u;
    return hash;
}

static void makeShortName(const char* long_name, char* out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    size_t pos = 0;
    if (long_name) {
        for (const char* p = long_name; *p && pos + 1 < out_len; ++p) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                out[pos++] = c;
            }
        }
    }
    if (pos == 0 && out_len > 1) out[pos++] = 'S';
    out[pos] = '\0';
}

static bool buildMeshtasticNodeInfoPayload(uint8_t* out, size_t out_len, size_t* written) {
    if (!out || !written) return false;
    meshtastic_User user = meshtastic_User_init_zero;
    snprintf(user.id, sizeof(user.id), "!%08lX",
             static_cast<unsigned long>(g_meshtastic.config().node_num));
    strncpy(user.long_name, own_name, sizeof(user.long_name) - 1);
    user.long_name[sizeof(user.long_name) - 1] = '\0';
    makeShortName(own_name, user.short_name, sizeof(user.short_name));
#if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    for (int i = 0; i < 6; ++i) {
        user.macaddr[5 - i] = static_cast<pb_byte_t>((mac >> (8 * i)) & 0xFF);
    }
#endif
    user.hw_model = meshtastic_HardwareModel_T_DECK;
    user.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
    return slopos::meshtastic::encodeProtoMessage(&meshtastic_User_msg, &user,
                                                  out, out_len, written);
}

static bool buildMeshtasticPositionPayload(uint8_t* out, size_t out_len, size_t* written) {
    if (!out || !written || !slopos_gps_has_fix()) return false;
    meshtastic_Position pos = meshtastic_Position_init_zero;
    pos.has_latitude_i = true;
    pos.latitude_i = static_cast<int32_t>(slopos_gps_latitude() * 10000000.0f);
    pos.has_longitude_i = true;
    pos.longitude_i = static_cast<int32_t>(slopos_gps_longitude() * 10000000.0f);
    pos.has_altitude = true;
    pos.altitude = static_cast<int32_t>(slopos_gps_altitude_m());
    pos.time = rtc_clock.getCurrentTime();
    pos.timestamp = pos.time;
    pos.location_source = meshtastic_Position_LocSource_LOC_INTERNAL;
    pos.altitude_source = meshtastic_Position_AltSource_ALT_INTERNAL;
    pos.has_ground_speed = true;
    pos.ground_speed = static_cast<uint32_t>(slopos_gps_speed_kn() * 0.514444f);
    pos.has_ground_track = true;
    pos.ground_track = static_cast<uint32_t>(slopos_gps_heading() * 100.0f);
    pos.sats_in_view = slopos_gps_satellites();
    pos.fix_quality = slopos_gps_fix_quality();
    return slopos::meshtastic::encodeProtoMessage(&meshtastic_Position_msg, &pos,
                                                  out, out_len, written);
}

static bool meshtasticStartFrame(const uint8_t* bytes, size_t len, const char* log_type) {
    if (!initialized || active_protocol != slopos::ProtocolMode::Meshtastic ||
        meshtastic_tx_pending || !bytes || len == 0 || len > slopos::meshtastic::kMaxLoRaFrameBytes) {
        return false;
    }
    bool ok = radio_driver.startSendRaw(bytes, static_cast<int>(len));
    if (ok) {
        meshtastic_tx_pending = true;
        slopos::mesh::pushPacketLog(own_name, 0, 0.0f, log_type ? log_type : "MTX");
    }
    return ok;
}

static bool meshtasticSendPayload(slopos::meshtastic::PortNum portnum,
                                  uint32_t to,
                                  const uint8_t* payload,
                                  size_t payload_len,
                                  bool want_response,
                                  bool want_ack,
                                  const char* log_type) {
    uint8_t frame[slopos::meshtastic::kMaxLoRaFrameBytes];
    size_t written = 0;
    if (!g_meshtastic.buildDataBytes(portnum, payload, payload_len, to,
                                     nextMeshtasticPacketId(), want_response, want_ack,
                                     frame, sizeof(frame), &written)) {
        return false;
    }
    return meshtasticStartFrame(frame, written, log_type);
}

static bool meshtasticSendPosition() {
    uint8_t payload[slopos::meshtastic::kDataPayloadLen];
    size_t payload_len = 0;
    if (!buildMeshtasticPositionPayload(payload, sizeof(payload), &payload_len)) return false;
    return meshtasticSendPayload(slopos::meshtastic::PortNum::Position,
                                 slopos::meshtastic::kBroadcastNode,
                                 payload, payload_len, false, false, "MTX_POS");
}

static bool meshtasticSendNodeInfo(bool want_replies) {
    uint8_t payload[slopos::meshtastic::kDataPayloadLen];
    size_t payload_len = 0;
    if (!buildMeshtasticNodeInfoPayload(payload, sizeof(payload), &payload_len)) return false;
    return meshtasticSendPayload(slopos::meshtastic::PortNum::NodeInfo,
                                 slopos::meshtastic::kBroadcastNode,
                                 payload, payload_len, want_replies, false, "MTX_INFO");
}

static void serviceLvglTimers() {
    static uint32_t last_lvgl = 0;
    uint32_t now = millis();
    if (now - last_lvgl > 20) {
        last_lvgl = now;
        lv_timer_handler();
    }
}

// ════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════

namespace slopos {
namespace mesh {

// ── Packet log ────────────────────────────────────
static constexpr int MAX_PACKET_LOG = 50;
static PacketLogEntry pkt_log[MAX_PACKET_LOG];
static int pkt_log_head = 0;
static int pkt_log_count = 0;

void pushPacketLog(const char* source, int rssi, float snr, const char* type) {
    if (!source || !type) return;
    PacketLogEntry& e = pkt_log[pkt_log_head];
    e.timestamp = getCurrentTime();
    strncpy(e.source, source, sizeof(e.source) - 1);
    e.source[sizeof(e.source) - 1] = '\0';
    e.rssi = rssi;
    e.snr = snr;
    strncpy(e.type, type, sizeof(e.type) - 1);
    e.type[sizeof(e.type) - 1] = '\0';
    pkt_log_head = (pkt_log_head + 1) % MAX_PACKET_LOG;
    if (pkt_log_count < MAX_PACKET_LOG) pkt_log_count++;
}

// Inject a simulated message into the queue (for remote test mode — no radio)
// All functions in this file that access the radio check for g_mesh == nullptr,
// so this is safe to call without radio initialisation.
void injectMessage(const char* sender, const char* channel, const char* text)
{
    if (!sender || !text) return;
    queue_push(sender, channel, text);
    if (!channel || channel[0] == '\0') {
        pushPacketLog(sender, -50, 8.0f, "DM");
    } else {
        pushPacketLog(sender, -50, 8.0f, "CHANNEL");
    }
#if SLOPOS_DEBUG_MESH
    SLOPOS_RUNTIME_FEAT(mesh) {
    Serial.printf("[test] injected msg from %s%s%s: %s\n",
                  sender, channel && channel[0] ? " in " : "",
                  channel && channel[0] ? channel : "", text);
    }
#endif
}

slopos::ProtocolMode getProtocolMode() {
    return active_protocol;
}

const char* getProtocolModeName() {
    return slopos::protocolModeName(active_protocol);
}

bool setProtocolMode(slopos::ProtocolMode mode) {
    slopos::NodePrefs p = slopos::prefs_get();
    p.protocol_mode = mode;
    slopos::prefs_set(p);
    active_protocol = mode;
    return true;
}

bool protocolSupportsDirectMessages() {
    return true;
}

bool protocolSupportsContacts() {
    return true;
}

bool protocolSupportsChannels() {
    return true;
}

bool protocolSupportsAdvert() {
    return true;
}

bool protocolSupportsTrace() {
    return true;
}

bool init(bool spiffs_ok)
{
    fallback_clock.begin();
    rtc_clock.begin(Wire);

    // ── Radio configuration: use compile-time defaults if not configured ──
    const slopos::NodePrefs& p = slopos::prefs_get();
    active_protocol = p.protocol_mode;
    if (p.node_name[0]) setOwnName(p.node_name);

    float   freq     = p.configured ? p.freq  : LORA_FREQ;
    float   bw       = p.configured ? p.bw    : LORA_BW;
    int     sf       = p.configured ? p.sf    : LORA_SF;
    int     cr       = p.configured ? p.cr    : LORA_CR;
    int     tx_power = p.configured ? p.tx_power_dbm : LORA_TX_PWR;

    if (!p.configured) {
#if SLOPOS_DEBUG_MESH
        Serial.println("[mesh] Using compile-time defaults — open Settings to customize");
#endif
    }

    // ── SX1262 hard reset: radio may retain state across ESP32 reboots.
    //     If BUSY pin is stuck HIGH from a previous crash, std_init() hangs
    //     in waitForBusyPin() → watchdog reset → infinite bootloop.
    //     Solution: assert RST LOW for 100µs, release, wait 10ms for TCXO.
#if SLOPOS_DEBUG_MESH
    Serial.println("[mesh] hard-resetting SX1262 via RST pin...");
#endif
    pinMode(P_LORA_RESET, OUTPUT);
    digitalWrite(P_LORA_RESET, LOW);
    delayMicroseconds(100);
    digitalWrite(P_LORA_RESET, HIGH);
    delay(10);  // TCXO stabilization + radio calibration

#if SLOPOS_DEBUG_MESH
    Serial.println("[mesh] initializing LoRa SPI bus...");
#endif
    lora_spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
#if SLOPOS_DEBUG_MESH
    Serial.println("[mesh] calling radio_module.std_init()...");
#endif
    if (!radio_module.std_init(&lora_spi)) {
        Serial.println("[mesh] ERROR: Radio init failed");
        return false;
    }

    fast_rng.begin(radio_module.random(0x7FFFFFFF));
    meshtastic_packet_id = static_cast<uint32_t>(radio_module.random(0x7FFFFFFF));
    if (meshtastic_packet_id == 0) meshtastic_packet_id = 1;

    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        slopos::meshtastic::MeshtasticNode::Config cfg;
        cfg.node_num = makeMeshtasticNodeNum(p);
        cfg.region = static_cast<slopos::meshtastic::RegionCode>(p.meshtastic_region);
        if (!slopos::meshtastic::getRegion(cfg.region)) cfg.region = slopos::meshtastic::RegionCode::US;
        cfg.preset = static_cast<slopos::meshtastic::ModemPreset>(p.meshtastic_preset);
        cfg.hop_limit = p.meshtastic_hop_limit;
        strncpy(cfg.channel_name, p.meshtastic_channel, sizeof(cfg.channel_name) - 1);
        cfg.channel_name[sizeof(cfg.channel_name) - 1] = '\0';
        memset(cfg.psk, 0, sizeof(cfg.psk));
        cfg.psk_len = p.meshtastic_psk_len;
        if (cfg.psk_len > sizeof(cfg.psk)) cfg.psk_len = 1;
        memcpy(cfg.psk, p.meshtastic_psk, cfg.psk_len);

        if (!g_meshtastic.begin(cfg) || !g_meshtastic.getFrequencyPlan(&meshtastic_plan)) {
            Serial.println("[mesh] ERROR: Meshtastic config failed");
            return false;
        }

        radio_module.setFrequency(meshtastic_plan.frequency_mhz);
        radio_module.setBandwidth(meshtastic_plan.params.bandwidth_khz);
        radio_module.setSpreadingFactor(meshtastic_plan.params.spreading_factor);
        radio_module.setCodingRate(meshtastic_plan.params.coding_rate);
        radio_module.setOutputPower(meshtastic_plan.tx_power_dbm);
        radio_module.setSyncWord(MESHTASTIC_SYNC_WORD);
        radio_module.setPreambleLength(16);
        radio_driver.begin();

        initialized = true;
#if SLOPOS_DEBUG_MESH
        Serial.printf("[mesh] Meshtastic: %.3f MHz / %.1f kHz / SF%d / CR4/%d / %d dBm\n",
                      meshtastic_plan.frequency_mhz,
                      meshtastic_plan.params.bandwidth_khz,
                      meshtastic_plan.params.spreading_factor,
                      meshtastic_plan.params.coding_rate,
                      meshtastic_plan.tx_power_dbm);
#endif
        pushPacketLog("SYSTEM", 0, 0.0f, "BOOT");
        meshtasticSendNodeInfo(true);
        return true;
    }

    radio_module.setFrequency(freq);
    radio_module.setBandwidth(bw);
    radio_module.setSpreadingFactor(sf);
    radio_module.setCodingRate(cr);   // denominator (5–8); RadioLib rejects the SX126X enum constants
    radio_module.setOutputPower(tx_power);
#if SLOPOS_DEBUG_MESH
    Serial.printf("[mesh] Radio: %.3f MHz / %.1f kHz / SF%d / CR4/%d / %d dBm\n",
                  freq, bw, sf, cr, tx_power);
#endif

    g_mesh = new SlopMesh(radio_driver, millis_clock, fast_rng, rtc_clock, pkt_mgr, tables);
    if (!g_mesh) {
        Serial.println("[mesh] ERROR: SlopMesh allocation failed");
        return false;
    }
    g_mesh->setMessageCallback(onMeshMessage);
    g_mesh->setOwnName(own_name);

    // Generate or load identity
    if (!loadIdentity(g_mesh->self_id)) {
        g_mesh->self_id = ::mesh::LocalIdentity(&fast_rng);
        if (spiffs_ok) {
            saveIdentity(g_mesh->self_id);
        } else {
            Serial.println("[mesh] WARNING: SPIFFS unavailable — identity is ephemeral");
        }
    }

    g_mesh->begin();

    // Restore persisted channels from NVS
    loadChannels();

    // Only broadcast advert if user has explicitly configured radio params.
    // Compile-time defaults may be illegal in some regions — transmit gating
    // prevents first-boot broadcasts until user opens Settings → Radio Setup.
    if (p.configured) {
        g_mesh->broadcastAdvert(own_name);
    }

    initialized = true;
#if SLOPOS_DEBUG_MESH
    Serial.println("[mesh] SlopMesh initialized");
#endif
    // Test entry to verify packet log works
    pushPacketLog("SYSTEM", 0, 0.0f, "BOOT");
    return true;
}

void loop()
{
    if (!initialized) return;

    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        radio_driver.loop();
        if (meshtastic_tx_pending && radio_driver.isSendComplete()) {
            radio_driver.onSendFinished();
            meshtastic_tx_pending = false;
            pushPacketLog(own_name, 0, 0.0f, "MTX_DONE");
            if (meshtastic_position_after_nodeinfo) {
                meshtastic_position_after_nodeinfo = false;
                meshtasticSendPosition();
            }
        }

        if (!meshtastic_tx_pending) {
            uint8_t bytes[slopos::meshtastic::kMaxLoRaFrameBytes];
            int len = radio_driver.recvRaw(bytes, sizeof(bytes));
            if (len > 0) {
                int rssi = static_cast<int>(radio_driver.getLastRSSI());
                float snr = radio_driver.getLastSNR();
                uint32_t now = rtc_clock.getCurrentTime();
                if (g_meshtastic.ingestBytes(bytes, static_cast<size_t>(len), rssi, snr, now)) {
                    pushPacketLog("Meshtastic", rssi, snr, "MRX");
                    slopos::meshtastic::MeshtasticNode::Message rx[4];
                    int n = g_meshtastic.pollMessages(rx, 4);
                    for (int i = 0; i < n; ++i) {
                        char fallback[16];
                        const char* sender = g_meshtastic.nameForNode(rx[i].from, fallback, sizeof(fallback));
                        queue_push(sender, rx[i].channel, rx[i].text);
                    }
                }
            }
        }

        rtc_clock.tick();
        serviceLvglTimers();
        return;
    }

    if (!g_mesh) return;
    g_mesh->loop();  // Dispatcher::loop() — fast, non-blocking
    rtc_clock.tick();
    serviceLvglTimers();
}

// ── Send ────────────────────────────────────────

bool sendMessage(const char* dest, const char* text) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        if (!dest || !text || meshtastic_tx_pending) return false;
        uint32_t node = 0;
        if (!g_meshtastic.nodeNumForContact(dest, &node)) return false;
        uint8_t frame[slopos::meshtastic::kMaxLoRaFrameBytes];
        size_t written = 0;
        if (!g_meshtastic.buildTextBytesTo(node, text, nextMeshtasticPacketId(),
                                           frame, sizeof(frame), &written)) {
            return false;
        }
        return meshtasticStartFrame(frame, written, "MTX_DM");
    }
    bool ok = g_mesh ? g_mesh->sendTextTo(dest, text) : false;
    if (ok) pushPacketLog(own_name, 0, 0.0f, "TX_DM");
    return ok;
}

bool sendChannelMessage(const char* channel_name, const char* text) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        if (!text || meshtastic_tx_pending) return false;
        (void)channel_name;
        uint8_t frame[slopos::meshtastic::kMaxLoRaFrameBytes];
        size_t written = 0;
        if (!g_meshtastic.buildTextBytes(text, nextMeshtasticPacketId(),
                                         frame, sizeof(frame), &written)) {
            return false;
        }
        return meshtasticStartFrame(frame, written, "MTX_CHAN");
    }
    if (!g_mesh) return false;
    for (int i = 0; i < g_mesh->getChannelCount(); i++) {
        auto* ch = g_mesh->getChannel(i);
        if (ch && strcmp(ch->name, channel_name) == 0) {
            bool ok = g_mesh->sendGroupText(i, text);
            if (ok) pushPacketLog(own_name, 0, 0.0f, "TX_CHAN");
            return ok;
        }
    }
    return false;
}

// ── Message queue ───────────────────────────────

int pollMessages(MeshMessage* out, int max) {
    int n = 0;
    while (n < max && queue_pop(&out[n])) n++;
    return n;
}

int pendingMessageCount() { return msg_count; }

// ── Contacts ────────────────────────────────────

int getContactCount() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        slopos::meshtastic::MeshtasticNode::Contact contacts[32];
        return g_meshtastic.exportContacts(contacts, 32);
    }
    return g_mesh ? g_mesh->getContactCount() : 0;
}

int exportContacts(char names[][32], int max) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        if (!names || max <= 0) return 0;
        slopos::meshtastic::MeshtasticNode::Contact contacts[32];
        int n = g_meshtastic.exportContacts(contacts, max < 32 ? max : 32);
        for (int i = 0; i < n; ++i) {
            strncpy(names[i], contacts[i].name, 31);
            names[i][31] = '\0';
        }
        return n;
    }
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getContactCount() && n < max; i++) {
        auto* c = g_mesh->getContact(i);
        if (c) { strncpy(names[n], c->name, 31); names[n][31] = '\0'; n++; }
    }
    return n;
}

// ContactInfo is declared in mesh_wrapper.h — exportContactsFull uses it

int exportContactsFull(ContactInfo* out, int max) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        if (!out || max <= 0) return 0;
        slopos::meshtastic::MeshtasticNode::Contact contacts[32];
        int n = g_meshtastic.exportContacts(contacts, max < 32 ? max : 32);
        for (int i = 0; i < n; ++i) {
            strncpy(out[i].name, contacts[i].name, sizeof(out[i].name) - 1);
            out[i].name[sizeof(out[i].name) - 1] = '\0';
            out[i].rssi = contacts[i].rssi;
            out[i].last_seen = contacts[i].last_seen;
        }
        return n;
    }
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getContactCount() && n < max; i++) {
        auto* c = g_mesh->getContact(i);
        if (c && c->name[0]) {
            strncpy(out[n].name, c->name, 31);
            out[n].name[31] = '\0';
            out[n].rssi = c->last_rssi;
            out[n].last_seen = c->last_seen;
            n++;
        }
    }
    return n;
}

// ── Channels ────────────────────────────────────

int getChannelCount() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) return g_meshtastic.configured() ? 1 : 0;
    return g_mesh ? g_mesh->getChannelCount() : 0;
}

int exportChannels(char names[][32], int max) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        if (!names || max <= 0 || !g_meshtastic.configured()) return 0;
        strncpy(names[0], g_meshtastic.config().channel_name, 31);
        names[0][31] = '\0';
        return 1;
    }
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getChannelCount() && n < max; i++) {
        auto* ch = g_mesh->getChannel(i);
        if (ch) { strncpy(names[n], ch->name, 31); names[n][31] = '\0'; n++; }
    }
    return n;
}

bool addChannel(const char* name, const char* psk) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        (void)psk;
        if (!name || !name[0]) return false;
        slopos::NodePrefs p = slopos::prefs_get();
        strncpy(p.meshtastic_channel, name[0] == '#' ? name + 1 : name,
                sizeof(p.meshtastic_channel) - 1);
        p.meshtastic_channel[sizeof(p.meshtastic_channel) - 1] = '\0';
        slopos::prefs_set(p);
        return true;
    }
    return g_mesh ? g_mesh->addChannel(name, psk) : false;
}

bool addHashtagChannel(const char* name) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        return addChannel(name, nullptr);
    }
    return g_mesh ? g_mesh->addHashtagChannel(name) : false;
}

bool joinPublicChannel() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) return g_meshtastic.configured();
    return addChannel("Public", "izOH6cXN6mrJ5e26oRXNcg==");
}

// ── Identity ────────────────────────────────────

void setOwnName(const char* name) {
    if (!name) return;
    strncpy(own_name, name, sizeof(own_name) - 1);
    own_name[sizeof(own_name) - 1] = '\0';
    if (g_mesh) g_mesh->setOwnName(own_name);
}

const char* getOwnName() { return own_name; }

// ── Radio stats ─────────────────────────────────

int getNoiseFloor()   { return initialized ? (int)radio_driver.getNoiseFloor() : -120; }
int getLastRSSI()     { return initialized ? (int)radio_driver.getLastRSSI() : 0; }
float getLastSNR()    { return initialized ? radio_driver.getLastSNR() : 0.0f; }

bool sendAdvert() {
    // Rate limit: reject calls within 10 seconds of the last successful advert.
    // The UI also enforces this via button cooldown, but programmatic
    // callers (e.g. Terminal's `advert` command) bypass that layer.
    static uint32_t last_advert_ms = 0;
    uint32_t now_ms = millis();
    if (last_advert_ms != 0 && now_ms - last_advert_ms < 10000) {
        return false;
    }

    bool has_fix = slopos_gps_has_fix();
    last_advert_time = getCurrentTime();
    last_advert_used_gps = has_fix;

    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        bool ok = meshtasticSendNodeInfo(true);
        meshtastic_position_after_nodeinfo = ok && has_fix;
        last_advert_success = ok;
        if (ok) last_advert_ms = now_ms;
        return ok;
    }

    if (!g_mesh) {
        last_advert_success = false;
        return false;
    }

    if (has_fix) {
        g_mesh->broadcastAdvert(own_name,
            slopos_gps_latitude(), slopos_gps_longitude());
    } else {
        g_mesh->broadcastAdvert(own_name);
    }

    last_advert_success = true;
    pushPacketLog(own_name, 0, 0.0f, "TX_ADV");
    last_advert_ms = now_ms;
    return true;
}

uint32_t getLastAdvertTime() {
    return last_advert_time;
}

bool getLastAdvertSuccess() {
    return last_advert_success;
}

bool getLastAdvertUsedGps() {
    return last_advert_used_gps;
}

uint32_t getCurrentTime() {
    return initialized ? rtc_clock.getCurrentTime() : 0;
}

bool setSystemTime(uint32_t epoch_seconds) {
    if (!initialized) return false;
    rtc_clock.setCurrentTime(epoch_seconds);
    fallback_clock.setCurrentTime(epoch_seconds);  // always keep soft RTC in sync too
    return true;
}

void getCurrentLocalDateTime(int* year, int* month, int* day, int* hour, int* minute) {
    if (!initialized || !year || !month || !day || !hour || !minute) {
        if (year) *year = 2024;
        if (month) *month = 1;
        if (day) *day = 1;
        if (hour) *hour = 0;
        if (minute) *minute = 0;
        return;
    }
    uint32_t epoch = rtc_clock.getCurrentTime();
    time_t t = epoch;
    struct tm* tm_info = gmtime(&t);
    *year   = tm_info->tm_year + 1900;
    *month  = tm_info->tm_mon + 1;
    *day    = tm_info->tm_mday;
    *hour   = tm_info->tm_hour;
    *minute = tm_info->tm_min;
}

uint32_t makeEpoch(int year, int month, int day, int hour, int minute) {
    struct tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = 0;
    tm.tm_isdst = 0;

    // Force UTC so that the wall-clock values the user typed become the correct epoch
    // (getCurrentLocalDateTime uses gmtime, so we must match on the write side)
    // Copy to stack buffer first — setenv() can invalidate the getenv() pointer on ESP32/newlib
    char old_tz_buf[64] = {};
    const char* old_tz_raw = getenv("TZ");
    if (old_tz_raw) strncpy(old_tz_buf, old_tz_raw, sizeof(old_tz_buf) - 1);
    const char* old_tz = old_tz_buf[0] ? old_tz_buf : nullptr;
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t t = mktime(&tm);
    if (old_tz) setenv("TZ", old_tz, 1); else unsetenv("TZ");
    tzset();

    return (uint32_t)t;
}

static uint32_t trace_tag_counter = 0;

bool sendTrace(int contact_idx, uint32_t* out_tag) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        slopos::meshtastic::MeshtasticNode::Contact contacts[32];
        int n = g_meshtastic.exportContacts(contacts, 32);
        if (contact_idx < 0 || contact_idx >= n) return false;
        meshtastic_RouteDiscovery route = meshtastic_RouteDiscovery_init_zero;
        uint8_t payload[slopos::meshtastic::kDataPayloadLen];
        size_t payload_len = 0;
        if (!slopos::meshtastic::encodeProtoMessage(&meshtastic_RouteDiscovery_msg, &route,
                                                    payload, sizeof(payload), &payload_len)) {
            return false;
        }
        uint32_t tag = ++trace_tag_counter;
        if (out_tag) *out_tag = tag;
        return meshtasticSendPayload(slopos::meshtastic::PortNum::TraceRoute,
                                     contacts[contact_idx].node_num,
                                     payload, payload_len, true, true, "MTX_TRACE");
    }
    if (!g_mesh) return false;
    uint32_t tag = ++trace_tag_counter;
    if (out_tag) *out_tag = tag;
    return g_mesh->sendTrace(contact_idx, tag);
}

bool hasTraceResult()   { return g_mesh ? g_mesh->hasTraceResult() : false; }
uint8_t getTracePathLen() { return g_mesh ? g_mesh->getTracePathLen() : 0; }
void getTracePath(uint8_t* snrs, uint8_t* hashes) {
    if (g_mesh) g_mesh->getTracePath(snrs, hashes);
}
void clearTraceResult() { if (g_mesh) g_mesh->clearTraceResult(); }

bool contactHasPath(int idx) {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        return idx >= 0 && idx < getContactCount();
    }
    if (!g_mesh) return false;
    auto* c = g_mesh->getContact(idx);
    return c && c->out_path_len != OUT_PATH_UNKNOWN;
}

// ── Ping Nearby ────────────────────────────────
bool sendPingNearby() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) {
        return meshtasticSendNodeInfo(true);
    }
    return g_mesh ? g_mesh->sendPingNearby() : false;
}

bool pingIsActive() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) return false;
    return g_mesh ? g_mesh->pingIsActive() : false;
}

bool pingOnCooldown() {
    if (active_protocol == slopos::ProtocolMode::Meshtastic) return false;
    return g_mesh ? g_mesh->pingOnCooldown() : false;
}

uint32_t pingCooldownRemaining() {
    return g_mesh ? g_mesh->pingCooldownRemaining() : 0;
}

int getPingResultCount() {
    return g_mesh ? g_mesh->getPingResultCount() : 0;
}

const PingResult* getPingResult(int i) {
    if (!g_mesh) return nullptr;
    auto* r = g_mesh->getPingResult(i);
    if (!r) return nullptr;
    // Return as public PingResult (same layout, but from different namespace)
    return reinterpret_cast<const PingResult*>(r);
}

void saveChannels() {
    if (!g_mesh) return;
    Preferences nvs;
    if (!nvs.begin("slopos", false)) return;
    int n = g_mesh->getChannelCount();
    nvs.putUChar("ch_cnt", (uint8_t)n);
    for (int i = 0; i < n; i++) {
        auto* ch = g_mesh->getChannel(i);
        if (!ch) continue;
        char key[16];
        snprintf(key, sizeof(key), "ch_%d_name", i);
        nvs.putString(key, ch->name);
        snprintf(key, sizeof(key), "ch_%d_sec", i);
        nvs.putBytes(key, ch->channel.secret, sizeof(ch->channel.secret));
        snprintf(key, sizeof(key), "ch_%d_hash", i);
        nvs.putBytes(key, ch->channel.hash, sizeof(ch->channel.hash));
    }
    nvs.end();
}

void loadChannels() {
    if (!g_mesh) return;
    Preferences nvs;
    if (!nvs.begin("slopos", true)) return;
    int n = nvs.getUChar("ch_cnt", 0);
    for (int i = 0; i < n; i++) {
        char key[16];
        char name[32];
        uint8_t secret[32];
        uint8_t hash[32];
        snprintf(key, sizeof(key), "ch_%d_name", i);
        nvs.getString(key, name, sizeof(name));
        snprintf(key, sizeof(key), "ch_%d_sec", i);
        nvs.getBytes(key, secret, sizeof(secret));
        snprintf(key, sizeof(key), "ch_%d_hash", i);
        nvs.getBytes(key, hash, sizeof(hash));
        if (name[0]) g_mesh->loadChannel(secret, sizeof(secret), hash, name);
    }
    nvs.end();
}

void saveState() {
    if (g_mesh) saveIdentity(g_mesh->self_id);
}

int getPacketLogCount() { return pkt_log_count; }

bool getPacketLogEntry(int index, PacketLogEntry* out) {
    if (index < 0 || index >= pkt_log_count || !out) return false;
    int idx = (pkt_log_head - pkt_log_count + index + MAX_PACKET_LOG) % MAX_PACKET_LOG;
    *out = pkt_log[idx];
    return true;
}

} // namespace mesh
} // namespace slopos
