// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include "mesh/mesh_wrapper.h"
#include <cstring>

namespace slopos::mesh {

static MeshMessage     mock_msgs[8];
static int             mock_msg_count = 0;
static char            mock_own_name[32] = "MockNode";
static int             mock_noise = -120;
static int             mock_rssi  = -80;
static float           mock_snr   = 5.0f;
static slopos::ProtocolMode mock_protocol = slopos::ProtocolMode::MeshCore;

void mock_push_packet(const char* source, int rssi, float snr, const char* type);
void mock_push_message(const char* sender, const char* text);

// ── Lifecycle ────────────────────────────────────

bool init(bool spiffs_ok) { (void)spiffs_ok; return true; }
void loop() {}

slopos::ProtocolMode getProtocolMode() { return mock_protocol; }
const char* getProtocolModeName() { return slopos::protocolModeName(mock_protocol); }
bool setProtocolMode(slopos::ProtocolMode mode) { mock_protocol = mode; return true; }
bool protocolSupportsDirectMessages() { return true; }
bool protocolSupportsContacts() { return true; }
bool protocolSupportsChannels() { return true; }
bool protocolSupportsAdvert() { return true; }
bool protocolSupportsTrace() { return true; }

// ── Send ─────────────────────────────────────────

bool sendMessage(const char* dest_name, const char* text) {
    (void)dest_name; (void)text; return false;
}

bool sendChannelMessage(const char* channel_name, const char* text) {
    (void)channel_name; (void)text; return false;
}

int pollMessages(MeshMessage* out, int max) {
    int drained = 0;
    while (drained < max && mock_msg_count > 0) {
        out[drained] = mock_msgs[--mock_msg_count];
        drained++;
    }
    return drained;
}

int pendingMessageCount() { return mock_msg_count; }

// ── Identity ─────────────────────────────────────

void setOwnName(const char* name) {
    if (name) {
        strncpy(mock_own_name, name, sizeof(mock_own_name) - 1);
        mock_own_name[sizeof(mock_own_name) - 1] = '\0';
    }
}

const char* getOwnName() { return mock_own_name; }

// ── Contacts / channels ──────────────────────────

int getContactCount() { return 0; }
int exportContacts(char names[][32], int max) { return 0; }
int exportContactsFull(ContactInfo* out, int max) { (void)out; (void)max; return 0; }
int getChannelCount() { return 0; }
int exportChannels(char names[][32], int max) { return 0; }
bool addChannel(const char* name, const char* psk) { return false; }
bool addHashtagChannel(const char* name) { (void)name; return false; }
bool joinPublicChannel() { return true; }

// ── Radio stats ──────────────────────────────────

int getNoiseFloor() { return mock_noise; }
int getLastRSSI()   { return mock_rssi; }
float getLastSNR()  { return mock_snr; }

bool sendAdvert() { return false; }
uint32_t getLastAdvertTime() { return 0; }
bool getLastAdvertSuccess() { return false; }
bool getLastAdvertUsedGps() { return false; }
void saveState() {}
void saveChannels() {}
void loadChannels() {}

uint32_t getCurrentTime() { return 0; }
bool setSystemTime(uint32_t epoch_seconds) { (void)epoch_seconds; return true; }
void getCurrentLocalDateTime(int* year, int* month, int* day, int* hour, int* minute) {
    if (year) *year = 2024;
    if (month) *month = 1;
    if (day) *day = 1;
    if (hour) *hour = 0;
    if (minute) *minute = 0;
}
uint32_t makeEpoch(int year, int month, int day, int hour, int minute) {
    (void)year; (void)month; (void)day; (void)hour; (void)minute;
    return 0;
}

// ── Packet log ──────────────────────────────────

static PacketLogEntry mock_pkt_log[8];
static int mock_pkt_count = 0;

int getPacketLogCount() { return mock_pkt_count; }
bool getPacketLogEntry(int index, PacketLogEntry* out) {
    if (index < 0 || index >= mock_pkt_count || !out) return false;
    *out = mock_pkt_log[index];
    return true;
}

void mock_push_packet(const char* source, int rssi, float snr, const char* type) {
    if (mock_pkt_count >= 8) return;
    PacketLogEntry& e = mock_pkt_log[mock_pkt_count++];
    strncpy(e.source, source, sizeof(e.source) - 1);
    e.source[sizeof(e.source) - 1] = '\0';
    e.rssi = rssi;
    e.snr = snr;
    strncpy(e.type, type, sizeof(e.type) - 1);
    e.type[sizeof(e.type) - 1] = '\0';
    e.timestamp = 0;
}

void mock_clear_packets() { mock_pkt_count = 0; }

void pushPacketLog(const char* source, int rssi, float snr, const char* type) {
    mock_push_packet(source, rssi, snr, type);
}

void injectMessage(const char* sender, const char* channel, const char* text) {
    (void)channel;
    mock_push_message(sender, text);
}

bool sendTrace(int contact_idx, uint32_t* out_tag) {
    (void)contact_idx;
    if (out_tag) *out_tag = 1;
    return false;
}
bool hasTraceResult() { return false; }
uint8_t getTracePathLen() { return 0; }
void getTracePath(uint8_t* snrs_out, uint8_t* hashes_out) { (void)snrs_out; (void)hashes_out; }
void clearTraceResult() {}
bool contactHasPath(int contact_idx) { (void)contact_idx; return false; }

bool sendPingNearby() { return false; }
bool pingIsActive() { return false; }
bool pingOnCooldown() { return false; }
uint32_t pingCooldownRemaining() { return 0; }
int getPingResultCount() { return 0; }
const PingResult* getPingResult(int i) { (void)i; return nullptr; }

// ── Test helpers ─────────────────────────────────

void mock_push_message(const char* sender, const char* text) {
    if (mock_msg_count >= 8) return;
    MeshMessage& m = mock_msgs[mock_msg_count++];
    strncpy(m.sender, sender, sizeof(m.sender) - 1);
    m.channel[0] = '\0';
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.timestamp = 0;
    m.is_self = false;
}

void mock_set_noise(int v)  { mock_noise = v; }
void mock_set_rssi(int v)   { mock_rssi = v; }
void mock_set_snr(float v)  { mock_snr = v; }

} // namespace slopos::mesh
