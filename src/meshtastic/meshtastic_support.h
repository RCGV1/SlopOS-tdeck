// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben
//
// Meshtastic wire-format helpers for SlopOS-TDeck.

#pragma once

#include <cstddef>
#include <cstdint>

#include <pb.h>
#include "meshtastic/config.pb.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/portnums.pb.h"

namespace slopos {
namespace meshtastic {

static constexpr size_t kHeaderLength = 16;
static constexpr size_t kMaxLoRaFrameBytes = 255;
static constexpr size_t kMaxEncryptedPayloadBytes = kMaxLoRaFrameBytes - kHeaderLength;
static constexpr size_t kDataPayloadLen = meshtastic_Constants_DATA_PAYLOAD_LEN;
static constexpr size_t kDataProtoMaxBytes = meshtastic_Data_size;
static constexpr size_t kMeshPacketProtoMaxBytes = meshtastic_MeshPacket_size;
static constexpr uint32_t kBroadcastNode = 0xFFFFFFFFu;
static constexpr uint8_t kNoNextHop = 0;
static constexpr uint8_t kNoRelayNode = 0;

static constexpr uint8_t kPacketFlagsHopLimitMask = 0x07;
static constexpr uint8_t kPacketFlagsWantAckMask = 0x08;
static constexpr uint8_t kPacketFlagsViaMqttMask = 0x10;
static constexpr uint8_t kPacketFlagsHopStartMask = 0xE0;
static constexpr uint8_t kPacketFlagsHopStartShift = 5;

enum class PortNum : uint16_t {
    Unknown = meshtastic_PortNum_UNKNOWN_APP,
    TextMessage = meshtastic_PortNum_TEXT_MESSAGE_APP,
    RemoteHardware = meshtastic_PortNum_REMOTE_HARDWARE_APP,
    Position = meshtastic_PortNum_POSITION_APP,
    NodeInfo = meshtastic_PortNum_NODEINFO_APP,
    Routing = meshtastic_PortNum_ROUTING_APP,
    Admin = meshtastic_PortNum_ADMIN_APP,
    TextMessageCompressed = meshtastic_PortNum_TEXT_MESSAGE_COMPRESSED_APP,
    Waypoint = meshtastic_PortNum_WAYPOINT_APP,
    Audio = meshtastic_PortNum_AUDIO_APP,
    DetectionSensor = meshtastic_PortNum_DETECTION_SENSOR_APP,
    Alert = meshtastic_PortNum_ALERT_APP,
    KeyVerification = meshtastic_PortNum_KEY_VERIFICATION_APP,
    RemoteShell = meshtastic_PortNum_REMOTE_SHELL_APP,
    Reply = meshtastic_PortNum_REPLY_APP,
    IpTunnel = meshtastic_PortNum_IP_TUNNEL_APP,
    PaxCounter = meshtastic_PortNum_PAXCOUNTER_APP,
    StoreForwardPlusPlus = meshtastic_PortNum_STORE_FORWARD_PLUSPLUS_APP,
    NodeStatus = meshtastic_PortNum_NODE_STATUS_APP,
    Serial = meshtastic_PortNum_SERIAL_APP,
    StoreForward = meshtastic_PortNum_STORE_FORWARD_APP,
    RangeTest = meshtastic_PortNum_RANGE_TEST_APP,
    Telemetry = meshtastic_PortNum_TELEMETRY_APP,
    Zps = meshtastic_PortNum_ZPS_APP,
    Simulator = meshtastic_PortNum_SIMULATOR_APP,
    TraceRoute = meshtastic_PortNum_TRACEROUTE_APP,
    NeighborInfo = meshtastic_PortNum_NEIGHBORINFO_APP,
    AtakPlugin = meshtastic_PortNum_ATAK_PLUGIN,
    MapReport = meshtastic_PortNum_MAP_REPORT_APP,
    PowerStress = meshtastic_PortNum_POWERSTRESS_APP,
    LoRaWanBridge = meshtastic_PortNum_LORAWAN_BRIDGE,
    ReticulumTunnel = meshtastic_PortNum_RETICULUM_TUNNEL_APP,
    Cayenne = meshtastic_PortNum_CAYENNE_APP,
    AtakPluginV2 = meshtastic_PortNum_ATAK_PLUGIN_V2,
    GroupAlarm = meshtastic_PortNum_GROUPALARM_APP,
    PrivateApp = meshtastic_PortNum_PRIVATE_APP,
    AtakForwarder = meshtastic_PortNum_ATAK_FORWARDER,
    Max = meshtastic_PortNum_MAX,
};

enum class ModemPreset : uint8_t {
    LongFast = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST,
    LongSlow = meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW,
    VeryLongSlow = meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW,
    MediumSlow = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW,
    MediumFast = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST,
    ShortSlow = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW,
    ShortFast = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST,
    LongModerate = meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE,
    ShortTurbo = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO,
    LongTurbo = meshtastic_Config_LoRaConfig_ModemPreset_LONG_TURBO,
    LiteFast = meshtastic_Config_LoRaConfig_ModemPreset_LITE_FAST,
    LiteSlow = meshtastic_Config_LoRaConfig_ModemPreset_LITE_SLOW,
    NarrowFast = meshtastic_Config_LoRaConfig_ModemPreset_NARROW_FAST,
    NarrowSlow = meshtastic_Config_LoRaConfig_ModemPreset_NARROW_SLOW,
};

enum class RegionCode : uint8_t {
    Unset = meshtastic_Config_LoRaConfig_RegionCode_UNSET,
    US = meshtastic_Config_LoRaConfig_RegionCode_US,
    EU433 = meshtastic_Config_LoRaConfig_RegionCode_EU_433,
    EU868 = meshtastic_Config_LoRaConfig_RegionCode_EU_868,
    CN = meshtastic_Config_LoRaConfig_RegionCode_CN,
    JP = meshtastic_Config_LoRaConfig_RegionCode_JP,
    ANZ = meshtastic_Config_LoRaConfig_RegionCode_ANZ,
    KR = meshtastic_Config_LoRaConfig_RegionCode_KR,
    TW = meshtastic_Config_LoRaConfig_RegionCode_TW,
    RU = meshtastic_Config_LoRaConfig_RegionCode_RU,
    IN = meshtastic_Config_LoRaConfig_RegionCode_IN,
    NZ865 = meshtastic_Config_LoRaConfig_RegionCode_NZ_865,
    TH = meshtastic_Config_LoRaConfig_RegionCode_TH,
    Lora24 = meshtastic_Config_LoRaConfig_RegionCode_LORA_24,
    UA433 = meshtastic_Config_LoRaConfig_RegionCode_UA_433,
    UA868 = meshtastic_Config_LoRaConfig_RegionCode_UA_868,
    MY433 = meshtastic_Config_LoRaConfig_RegionCode_MY_433,
    MY919 = meshtastic_Config_LoRaConfig_RegionCode_MY_919,
    SG923 = meshtastic_Config_LoRaConfig_RegionCode_SG_923,
    PH433 = meshtastic_Config_LoRaConfig_RegionCode_PH_433,
    PH868 = meshtastic_Config_LoRaConfig_RegionCode_PH_868,
    PH915 = meshtastic_Config_LoRaConfig_RegionCode_PH_915,
    ANZ433 = meshtastic_Config_LoRaConfig_RegionCode_ANZ_433,
    KZ433 = meshtastic_Config_LoRaConfig_RegionCode_KZ_433,
    KZ863 = meshtastic_Config_LoRaConfig_RegionCode_KZ_863,
    NP865 = meshtastic_Config_LoRaConfig_RegionCode_NP_865,
    BR902 = meshtastic_Config_LoRaConfig_RegionCode_BR_902,
    ITU1_2M = meshtastic_Config_LoRaConfig_RegionCode_ITU1_2M,
    ITU23_2M = meshtastic_Config_LoRaConfig_RegionCode_ITU23_2M,
    EU866 = meshtastic_Config_LoRaConfig_RegionCode_EU_866,
    EU874 = meshtastic_Config_LoRaConfig_RegionCode_EU_874,
    EU917 = meshtastic_Config_LoRaConfig_RegionCode_EU_917,
    EUN868 = meshtastic_Config_LoRaConfig_RegionCode_EU_N_868,
};

struct PacketHeader {
    uint32_t to = kBroadcastNode;
    uint32_t from = 0;
    uint32_t id = 0;
    uint8_t flags = 0;
    uint8_t channel = 0;
    uint8_t next_hop = kNoNextHop;
    uint8_t relay_node = kNoRelayNode;
};

struct DataPacket {
    PortNum portnum = PortNum::Unknown;
    uint8_t payload[kDataPayloadLen] = {};
    size_t payload_len = 0;
    bool want_response = false;
    uint32_t dest = 0;
    uint32_t source = 0;
    uint32_t request_id = 0;
    uint32_t reply_id = 0;
    uint32_t emoji = 0;
    bool has_bitfield = false;
    uint32_t bitfield = 0;
};

struct PacketFrame {
    PacketHeader header;
    uint8_t payload[kMaxEncryptedPayloadBytes] = {};
    size_t payload_len = 0;
};

struct RadioParams {
    float bandwidth_khz;
    uint8_t spreading_factor;
    uint8_t coding_rate;
};

struct RegionInfo {
    RegionCode code;
    const char* name;
    float freq_start_mhz;
    float freq_end_mhz;
    float duty_cycle;
    uint8_t power_limit_dbm;
    bool frequency_switching;
    bool wide_lora;
    ModemPreset default_preset;
    int16_t override_slot;
    float spacing_mhz;
    float padding_mhz;
};

struct FrequencyPlan {
    RadioParams params;
    float frequency_mhz;
    uint32_t slot;
    uint32_t slot_count;
    int8_t tx_power_dbm;
};

uint8_t makeFlags(uint8_t hop_limit, bool want_ack = false, bool via_mqtt = false, uint8_t hop_start = 0);
uint8_t getHopLimit(uint8_t flags);
uint8_t getHopStart(uint8_t flags);
bool getWantAck(uint8_t flags);
bool getViaMqtt(uint8_t flags);

bool encodeHeader(const PacketHeader& header, uint8_t* out, size_t out_len);
bool decodeHeader(const uint8_t* data, size_t len, PacketHeader* out);

bool encodeFrame(const PacketFrame& frame, uint8_t* out, size_t out_len, size_t* written);
bool decodeFrame(const uint8_t* data, size_t len, PacketFrame* out);

bool encodeData(const DataPacket& data, uint8_t* out, size_t out_len, size_t* written);
bool decodeData(const uint8_t* data, size_t len, DataPacket* out);
bool toProtoData(const DataPacket& data, meshtastic_Data* out);
bool fromProtoData(const meshtastic_Data& data, DataPacket* out);
bool encodeProtoData(const meshtastic_Data& data, uint8_t* out, size_t out_len, size_t* written);
bool decodeProtoData(const uint8_t* data, size_t len, meshtastic_Data* out);
bool frameToMeshPacket(const PacketFrame& frame, bool payload_encrypted, meshtastic_MeshPacket* out);
bool meshPacketToFrame(const meshtastic_MeshPacket& packet, PacketFrame* out);
bool encodeMeshPacketProto(const meshtastic_MeshPacket& packet, uint8_t* out, size_t out_len, size_t* written);
bool decodeMeshPacketProto(const uint8_t* data, size_t len, meshtastic_MeshPacket* out);
bool encodeProtoMessage(const pb_msgdesc_t* fields, const void* src, uint8_t* out, size_t out_len, size_t* written);
bool decodeProtoMessage(const pb_msgdesc_t* fields, const uint8_t* data, size_t len, void* out);
bool makeTextData(const char* text, DataPacket* out);
bool extractText(const DataPacket& data, char* out, size_t out_len);

const char* portName(PortNum port);
const char* presetName(ModemPreset preset, bool short_name = false);
bool presetParams(ModemPreset preset, bool wide_lora, RadioParams* out);

const RegionInfo* getRegion(RegionCode code);
const RegionInfo* getRegionByName(const char* name);
bool computeFrequencyPlan(RegionCode region,
                          ModemPreset preset,
                          const char* channel_name,
                          uint32_t channel_num,
                          float override_frequency_mhz,
                          float frequency_offset_mhz,
                          FrequencyPlan* out);

uint32_t djb2Hash(const char* text);
uint8_t xorHash(const uint8_t* data, size_t len);
bool expandPsk(const uint8_t* psk, size_t psk_len, uint8_t* out, size_t* out_len);
bool defaultPsk(uint8_t* out, size_t* out_len);
bool channelHash(const char* channel_name, const uint8_t* psk, size_t psk_len, uint8_t* out_hash);

bool cryptoAvailable();
bool applyAesCtr(uint8_t* bytes, size_t len, const uint8_t* key, size_t key_len, uint32_t from_node, uint64_t packet_id);
bool encodeEncryptedDataFrame(const PacketHeader& header,
                              const DataPacket& data,
                              const uint8_t* psk,
                              size_t psk_len,
                              PacketFrame* out);
bool decodeEncryptedDataFrame(const PacketFrame& frame,
                              const uint8_t* psk,
                              size_t psk_len,
                              DataPacket* out);

} // namespace meshtastic
} // namespace slopos
