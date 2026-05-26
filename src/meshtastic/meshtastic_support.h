// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben
//
// Meshtastic wire-format helpers for SlopOS-TDeck.

#pragma once

#include <cstddef>
#include <cstdint>

namespace slopos {
namespace meshtastic {

static constexpr size_t kHeaderLength = 16;
static constexpr size_t kMaxLoRaFrameBytes = 255;
static constexpr size_t kMaxEncryptedPayloadBytes = kMaxLoRaFrameBytes - kHeaderLength;
static constexpr size_t kDataPayloadLen = 233;
static constexpr uint32_t kBroadcastNode = 0xFFFFFFFFu;
static constexpr uint8_t kNoNextHop = 0;
static constexpr uint8_t kNoRelayNode = 0;

static constexpr uint8_t kPacketFlagsHopLimitMask = 0x07;
static constexpr uint8_t kPacketFlagsWantAckMask = 0x08;
static constexpr uint8_t kPacketFlagsViaMqttMask = 0x10;
static constexpr uint8_t kPacketFlagsHopStartMask = 0xE0;
static constexpr uint8_t kPacketFlagsHopStartShift = 5;

enum class PortNum : uint16_t {
    Unknown = 0,
    TextMessage = 1,
    RemoteHardware = 2,
    Position = 3,
    NodeInfo = 4,
    Routing = 5,
    Admin = 6,
    TextMessageCompressed = 7,
    Waypoint = 8,
    Audio = 9,
    DetectionSensor = 10,
    Alert = 11,
    KeyVerification = 12,
    RemoteShell = 13,
    Reply = 32,
    IpTunnel = 33,
    PaxCounter = 34,
    StoreForwardPlusPlus = 35,
    NodeStatus = 36,
    Serial = 64,
    StoreForward = 65,
    RangeTest = 66,
    Telemetry = 67,
    Zps = 68,
    Simulator = 69,
    TraceRoute = 70,
    NeighborInfo = 71,
    AtakPlugin = 72,
    MapReport = 73,
    PowerStress = 74,
    LoRaWanBridge = 75,
    ReticulumTunnel = 76,
    Cayenne = 77,
    AtakPluginV2 = 78,
    GroupAlarm = 112,
    PrivateApp = 256,
    AtakForwarder = 257,
    Max = 511,
};

enum class ModemPreset : uint8_t {
    LongFast = 0,
    LongSlow = 1,
    VeryLongSlow = 2,
    MediumSlow = 3,
    MediumFast = 4,
    ShortSlow = 5,
    ShortFast = 6,
    LongModerate = 7,
    ShortTurbo = 8,
    LongTurbo = 9,
    LiteFast = 10,
    LiteSlow = 11,
    NarrowFast = 12,
    NarrowSlow = 13,
};

enum class RegionCode : uint8_t {
    Unset = 0,
    US = 1,
    EU433 = 2,
    EU868 = 3,
    CN = 4,
    JP = 5,
    ANZ = 6,
    KR = 7,
    TW = 8,
    RU = 9,
    IN = 10,
    NZ865 = 11,
    TH = 12,
    Lora24 = 13,
    UA433 = 14,
    UA868 = 15,
    MY433 = 16,
    MY919 = 17,
    SG923 = 18,
    PH433 = 19,
    PH868 = 20,
    PH915 = 21,
    ANZ433 = 22,
    KZ433 = 23,
    KZ863 = 24,
    NP865 = 25,
    BR902 = 26,
    ITU1_2M = 27,
    ITU23_2M = 28,
    EU866 = 29,
    EU874 = 30,
    EU917 = 31,
    EUN868 = 32,
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
