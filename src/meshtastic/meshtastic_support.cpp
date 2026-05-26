// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "meshtastic_support.h"

#include <cstring>

#include <AES.h>
#include <CTR.h>
#include <pb_decode.h>
#include <pb_encode.h>
#define SLOPOS_MESHTASTIC_HAS_CRYPTO 1

namespace slopos {
namespace meshtastic {

namespace {

static const uint8_t kDefaultPskBytes[] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
};

static constexpr ModemPreset kStdPresets[] = {
    ModemPreset::LongFast,
    ModemPreset::LongSlow,
    ModemPreset::MediumSlow,
    ModemPreset::MediumFast,
    ModemPreset::ShortSlow,
    ModemPreset::ShortFast,
    ModemPreset::LongModerate,
    ModemPreset::ShortTurbo,
    ModemPreset::LongTurbo,
};

static constexpr ModemPreset kEu868Presets[] = {
    ModemPreset::LongFast,
    ModemPreset::LongSlow,
    ModemPreset::MediumSlow,
    ModemPreset::MediumFast,
    ModemPreset::ShortSlow,
    ModemPreset::ShortFast,
    ModemPreset::LongModerate,
};

static constexpr ModemPreset kLitePresets[] = {
    ModemPreset::LiteFast,
    ModemPreset::LiteSlow,
};

static constexpr ModemPreset kNarrowPresets[] = {
    ModemPreset::NarrowFast,
    ModemPreset::NarrowSlow,
};

struct ProfileInfo {
    const ModemPreset* presets;
    size_t preset_count;
    float spacing_mhz;
    float padding_mhz;
    int16_t override_slot;
};

static constexpr ProfileInfo kProfileStd = {kStdPresets, sizeof(kStdPresets) / sizeof(kStdPresets[0]), 0.0f, 0.0f, 0};
static constexpr ProfileInfo kProfileEu868 = {kEu868Presets, sizeof(kEu868Presets) / sizeof(kEu868Presets[0]), 0.0f, 0.0f, 0};
static constexpr ProfileInfo kProfileLite = {kLitePresets, sizeof(kLitePresets) / sizeof(kLitePresets[0]), 0.4f, 0.0375f, 0};
static constexpr ProfileInfo kProfileNarrow = {kNarrowPresets, sizeof(kNarrowPresets) / sizeof(kNarrowPresets[0]), 0.0f, 0.0104f, 1};

static constexpr RegionInfo kRegions[] = {
    {RegionCode::US, "US", 902.0f, 928.0f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::EU433, "EU_433", 433.0f, 434.0f, 10.0f, 10, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::EU868, "EU_868", 869.4f, 869.65f, 10.0f, 27, false, false, ModemPreset::LongFast, kProfileEu868.override_slot, kProfileEu868.spacing_mhz, kProfileEu868.padding_mhz},
    {RegionCode::CN, "CN", 470.0f, 510.0f, 100.0f, 19, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::JP, "JP", 920.5f, 923.5f, 100.0f, 13, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::ANZ, "ANZ", 915.0f, 928.0f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::ANZ433, "ANZ_433", 433.05f, 434.79f, 100.0f, 14, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::RU, "RU", 868.7f, 869.2f, 100.0f, 20, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::KR, "KR", 920.0f, 923.0f, 100.0f, 23, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::TW, "TW", 920.0f, 925.0f, 100.0f, 27, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::IN, "IN", 865.0f, 867.0f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::NZ865, "NZ_865", 864.0f, 868.0f, 100.0f, 36, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::TH, "TH", 920.0f, 925.0f, 10.0f, 27, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::UA433, "UA_433", 433.0f, 434.7f, 10.0f, 10, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::UA868, "UA_868", 868.0f, 868.6f, 1.0f, 14, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::MY433, "MY_433", 433.0f, 435.0f, 100.0f, 20, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::MY919, "MY_919", 919.0f, 924.0f, 100.0f, 27, true, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::SG923, "SG_923", 917.0f, 925.0f, 100.0f, 20, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::PH433, "PH_433", 433.0f, 434.7f, 100.0f, 10, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::PH868, "PH_868", 868.0f, 869.4f, 100.0f, 14, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::PH915, "PH_915", 915.0f, 918.0f, 100.0f, 24, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::KZ433, "KZ_433", 433.075f, 434.775f, 100.0f, 10, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::KZ863, "KZ_863", 863.0f, 868.0f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::NP865, "NP_865", 865.0f, 868.0f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::BR902, "BR_902", 902.0f, 907.5f, 100.0f, 30, false, false, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::Lora24, "LORA_24", 2400.0f, 2483.5f, 100.0f, 10, false, true, ModemPreset::LongFast, kProfileStd.override_slot, kProfileStd.spacing_mhz, kProfileStd.padding_mhz},
    {RegionCode::EU866, "EU_866", 865.6f, 867.6f, 2.5f, 27, false, false, ModemPreset::LiteFast, kProfileLite.override_slot, kProfileLite.spacing_mhz, kProfileLite.padding_mhz},
    {RegionCode::EUN868, "EU_N_868", 869.4f, 869.65f, 10.0f, 27, false, false, ModemPreset::NarrowSlow, kProfileNarrow.override_slot, kProfileNarrow.spacing_mhz, kProfileNarrow.padding_mhz},
    {RegionCode::Unset, "UNSET", 902.0f, 928.0f, 100.0f, 30, false, false, ModemPreset::LongFast, 0, 0.0f, 0.0f},
};

static void putLe32(uint8_t* out, uint32_t value)
{
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

static uint32_t getLe32(const uint8_t* in)
{
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

static void putLe64(uint8_t* out, uint64_t value)
{
    for (size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

static bool asciiEqualsIgnoreCase(const char* a, const char* b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca = static_cast<char>(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = static_cast<char>(cb - 'a' + 'A');
        if (ca != cb) return false;
    }
    return *a == '\0' && *b == '\0';
}

static const ProfileInfo* profileForRegion(const RegionInfo& region)
{
    if (region.code == RegionCode::EU868) return &kProfileEu868;
    if (region.code == RegionCode::EU866) return &kProfileLite;
    if (region.code == RegionCode::EUN868) return &kProfileNarrow;
    return &kProfileStd;
}

static bool presetAllowed(const RegionInfo& region, ModemPreset preset)
{
    const ProfileInfo* profile = profileForRegion(region);
    for (size_t i = 0; i < profile->preset_count; ++i) {
        if (profile->presets[i] == preset) return true;
    }
    return false;
}

} // namespace

uint8_t makeFlags(uint8_t hop_limit, bool want_ack, bool via_mqtt, uint8_t hop_start)
{
    uint8_t flags = hop_limit & kPacketFlagsHopLimitMask;
    if (want_ack) flags |= kPacketFlagsWantAckMask;
    if (via_mqtt) flags |= kPacketFlagsViaMqttMask;
    flags |= static_cast<uint8_t>((hop_start & 0x07) << kPacketFlagsHopStartShift);
    return flags;
}

uint8_t getHopLimit(uint8_t flags) { return flags & kPacketFlagsHopLimitMask; }
uint8_t getHopStart(uint8_t flags) { return (flags & kPacketFlagsHopStartMask) >> kPacketFlagsHopStartShift; }
bool getWantAck(uint8_t flags) { return (flags & kPacketFlagsWantAckMask) != 0; }
bool getViaMqtt(uint8_t flags) { return (flags & kPacketFlagsViaMqttMask) != 0; }

bool encodeHeader(const PacketHeader& header, uint8_t* out, size_t out_len)
{
    if (!out || out_len < kHeaderLength) return false;
    putLe32(out, header.to);
    putLe32(out + 4, header.from);
    putLe32(out + 8, header.id);
    out[12] = header.flags;
    out[13] = header.channel;
    out[14] = header.next_hop;
    out[15] = header.relay_node;
    return true;
}

bool decodeHeader(const uint8_t* data, size_t len, PacketHeader* out)
{
    if (!data || !out || len < kHeaderLength) return false;
    out->to = getLe32(data);
    out->from = getLe32(data + 4);
    out->id = getLe32(data + 8);
    out->flags = data[12];
    out->channel = data[13];
    out->next_hop = data[14];
    out->relay_node = data[15];
    return true;
}

bool encodeFrame(const PacketFrame& frame, uint8_t* out, size_t out_len, size_t* written)
{
    if (!out || !written || frame.payload_len > kMaxEncryptedPayloadBytes) return false;
    size_t total = kHeaderLength + frame.payload_len;
    if (total > out_len || total > kMaxLoRaFrameBytes) return false;
    if (!encodeHeader(frame.header, out, out_len)) return false;
    if (frame.payload_len > 0) memcpy(out + kHeaderLength, frame.payload, frame.payload_len);
    *written = total;
    return true;
}

bool decodeFrame(const uint8_t* data, size_t len, PacketFrame* out)
{
    if (!data || !out || len < kHeaderLength || len > kMaxLoRaFrameBytes) return false;
    if (!decodeHeader(data, len, &out->header)) return false;
    out->payload_len = len - kHeaderLength;
    if (out->payload_len > 0) memcpy(out->payload, data + kHeaderLength, out->payload_len);
    return true;
}

bool encodeData(const DataPacket& data, uint8_t* out, size_t out_len, size_t* written)
{
    meshtastic_Data proto = meshtastic_Data_init_zero;
    return toProtoData(data, &proto) && encodeProtoData(proto, out, out_len, written);
}

bool decodeData(const uint8_t* data, size_t len, DataPacket* out)
{
    meshtastic_Data proto = meshtastic_Data_init_zero;
    return decodeProtoData(data, len, &proto) && fromProtoData(proto, out);
}

bool toProtoData(const DataPacket& data, meshtastic_Data* out)
{
    if (!out || data.payload_len > kDataPayloadLen) return false;
    if (static_cast<uint32_t>(data.portnum) > static_cast<uint32_t>(PortNum::Max)) return false;
    if (data.bitfield > 0xFFu) return false;
    *out = meshtastic_Data_init_zero;
    out->portnum = static_cast<meshtastic_PortNum>(static_cast<uint16_t>(data.portnum));
    out->payload.size = data.payload_len;
    if (data.payload_len > 0) memcpy(out->payload.bytes, data.payload, data.payload_len);
    out->want_response = data.want_response;
    out->dest = data.dest;
    out->source = data.source;
    out->request_id = data.request_id;
    out->reply_id = data.reply_id;
    out->emoji = data.emoji;
    out->has_bitfield = data.has_bitfield;
    out->bitfield = static_cast<uint8_t>(data.bitfield);
    return true;
}

bool fromProtoData(const meshtastic_Data& data, DataPacket* out)
{
    if (!out || data.payload.size > kDataPayloadLen) return false;
    if (static_cast<uint32_t>(data.portnum) > static_cast<uint32_t>(meshtastic_PortNum_MAX)) return false;
    *out = DataPacket{};
    out->portnum = static_cast<PortNum>(static_cast<uint16_t>(data.portnum));
    out->payload_len = data.payload.size;
    if (out->payload_len > 0) memcpy(out->payload, data.payload.bytes, out->payload_len);
    out->want_response = data.want_response;
    out->dest = data.dest;
    out->source = data.source;
    out->request_id = data.request_id;
    out->reply_id = data.reply_id;
    out->emoji = data.emoji;
    out->has_bitfield = data.has_bitfield;
    out->bitfield = data.bitfield;
    return true;
}

bool encodeProtoData(const meshtastic_Data& data, uint8_t* out, size_t out_len, size_t* written)
{
    return encodeProtoMessage(&meshtastic_Data_msg, &data, out, out_len, written);
}

bool decodeProtoData(const uint8_t* data, size_t len, meshtastic_Data* out)
{
    if (!out) return false;
    *out = meshtastic_Data_init_zero;
    return decodeProtoMessage(&meshtastic_Data_msg, data, len, out);
}

bool frameToMeshPacket(const PacketFrame& frame, bool payload_encrypted, meshtastic_MeshPacket* out)
{
    if (!out || frame.payload_len > kMaxEncryptedPayloadBytes) return false;
    *out = meshtastic_MeshPacket_init_zero;
    out->from = frame.header.from;
    out->to = frame.header.to;
    out->channel = frame.header.channel;
    out->id = frame.header.id;
    out->hop_limit = getHopLimit(frame.header.flags);
    out->want_ack = getWantAck(frame.header.flags);
    out->via_mqtt = getViaMqtt(frame.header.flags);
    out->hop_start = getHopStart(frame.header.flags);
    out->next_hop = frame.header.next_hop;
    out->relay_node = frame.header.relay_node;
    out->transport_mechanism = meshtastic_MeshPacket_TransportMechanism_TRANSPORT_LORA;

    if (payload_encrypted) {
        if (frame.payload_len > sizeof(out->encrypted.bytes)) return false;
        out->which_payload_variant = meshtastic_MeshPacket_encrypted_tag;
        out->encrypted.size = frame.payload_len;
        if (frame.payload_len > 0) memcpy(out->encrypted.bytes, frame.payload, frame.payload_len);
        return true;
    }

    out->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    return decodeProtoData(frame.payload, frame.payload_len, &out->decoded);
}

bool meshPacketToFrame(const meshtastic_MeshPacket& packet, PacketFrame* out)
{
    if (!out) return false;
    *out = PacketFrame{};
    out->header.to = packet.to;
    out->header.from = packet.from;
    out->header.id = packet.id;
    out->header.flags = makeFlags(packet.hop_limit, packet.want_ack, packet.via_mqtt, packet.hop_start);
    out->header.channel = packet.channel;
    out->header.next_hop = packet.next_hop;
    out->header.relay_node = packet.relay_node;

    if (packet.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
        if (packet.encrypted.size > kMaxEncryptedPayloadBytes) return false;
        out->payload_len = packet.encrypted.size;
        if (out->payload_len > 0) memcpy(out->payload, packet.encrypted.bytes, out->payload_len);
        return true;
    }
    if (packet.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
        return encodeProtoData(packet.decoded, out->payload, sizeof(out->payload), &out->payload_len);
    }
    return false;
}

bool encodeMeshPacketProto(const meshtastic_MeshPacket& packet, uint8_t* out, size_t out_len, size_t* written)
{
    return encodeProtoMessage(&meshtastic_MeshPacket_msg, &packet, out, out_len, written);
}

bool decodeMeshPacketProto(const uint8_t* data, size_t len, meshtastic_MeshPacket* out)
{
    if (!out) return false;
    *out = meshtastic_MeshPacket_init_zero;
    return decodeProtoMessage(&meshtastic_MeshPacket_msg, data, len, out);
}

bool encodeProtoMessage(const pb_msgdesc_t* fields, const void* src, uint8_t* out, size_t out_len, size_t* written)
{
    if (!fields || !src || !out || !written) return false;
    pb_ostream_t stream = pb_ostream_from_buffer(out, out_len);
    if (!pb_encode(&stream, fields, src)) return false;
    *written = stream.bytes_written;
    return true;
}

bool decodeProtoMessage(const pb_msgdesc_t* fields, const uint8_t* data, size_t len, void* out)
{
    if (!fields || !data || !out) return false;
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    return pb_decode(&stream, fields, out);
}

bool makeTextData(const char* text, DataPacket* out)
{
    if (!text || !out) return false;
    size_t len = strlen(text);
    if (len > kDataPayloadLen) return false;
    *out = DataPacket{};
    out->portnum = PortNum::TextMessage;
    out->payload_len = len;
    if (len > 0) memcpy(out->payload, text, len);
    return true;
}

bool extractText(const DataPacket& data, char* out, size_t out_len)
{
    if (!out || out_len == 0 || data.payload_len >= out_len) return false;
    if (data.portnum != PortNum::TextMessage && data.portnum != PortNum::TextMessageCompressed) return false;
    if (data.payload_len > 0) memcpy(out, data.payload, data.payload_len);
    out[data.payload_len] = '\0';
    return true;
}

const char* portName(PortNum port)
{
    switch (port) {
    case PortNum::TextMessage: return "TEXT_MESSAGE_APP";
    case PortNum::TextMessageCompressed: return "TEXT_MESSAGE_COMPRESSED_APP";
    case PortNum::Position: return "POSITION_APP";
    case PortNum::NodeInfo: return "NODEINFO_APP";
    case PortNum::Routing: return "ROUTING_APP";
    case PortNum::Admin: return "ADMIN_APP";
    case PortNum::Telemetry: return "TELEMETRY_APP";
    case PortNum::TraceRoute: return "TRACEROUTE_APP";
    case PortNum::NeighborInfo: return "NEIGHBORINFO_APP";
    default: return "UNKNOWN_APP";
    }
}

const char* presetName(ModemPreset preset, bool short_name)
{
    switch (preset) {
    case ModemPreset::ShortTurbo: return short_name ? "ShortT" : "ShortTurbo";
    case ModemPreset::ShortSlow: return short_name ? "ShortS" : "ShortSlow";
    case ModemPreset::ShortFast: return short_name ? "ShortF" : "ShortFast";
    case ModemPreset::MediumSlow: return short_name ? "MedS" : "MediumSlow";
    case ModemPreset::MediumFast: return short_name ? "MedF" : "MediumFast";
    case ModemPreset::LongSlow: return short_name ? "LongS" : "LongSlow";
    case ModemPreset::LongFast: return short_name ? "LongF" : "LongFast";
    case ModemPreset::LongTurbo: return short_name ? "LongT" : "LongTurbo";
    case ModemPreset::LongModerate: return short_name ? "LongM" : "LongMod";
    case ModemPreset::LiteFast: return short_name ? "LiteF" : "LiteFast";
    case ModemPreset::LiteSlow: return short_name ? "LiteS" : "LiteSlow";
    case ModemPreset::NarrowFast: return short_name ? "NarF" : "NarrowFast";
    case ModemPreset::NarrowSlow: return short_name ? "NarS" : "NarrowSlow";
    default: return short_name ? "Custom" : "Invalid";
    }
}

bool presetParams(ModemPreset preset, bool wide_lora, RadioParams* out)
{
    if (!out) return false;
    switch (preset) {
    case ModemPreset::ShortTurbo:
        *out = {wide_lora ? 1625.0f : 500.0f, 7, 5};
        break;
    case ModemPreset::ShortFast:
        *out = {wide_lora ? 812.5f : 250.0f, 7, 5};
        break;
    case ModemPreset::ShortSlow:
        *out = {wide_lora ? 812.5f : 250.0f, 8, 5};
        break;
    case ModemPreset::MediumFast:
        *out = {wide_lora ? 812.5f : 250.0f, 9, 5};
        break;
    case ModemPreset::MediumSlow:
        *out = {wide_lora ? 812.5f : 250.0f, 10, 5};
        break;
    case ModemPreset::LongTurbo:
        *out = {wide_lora ? 1625.0f : 500.0f, 11, 8};
        break;
    case ModemPreset::LongModerate:
        *out = {wide_lora ? 406.25f : 125.0f, 11, 8};
        break;
    case ModemPreset::LongSlow:
        *out = {wide_lora ? 406.25f : 125.0f, 12, 8};
        break;
    case ModemPreset::LiteFast:
        *out = {125.0f, 9, 5};
        break;
    case ModemPreset::LiteSlow:
        *out = {125.0f, 10, 5};
        break;
    case ModemPreset::NarrowFast:
        *out = {62.5f, 7, 6};
        break;
    case ModemPreset::NarrowSlow:
        *out = {62.5f, 8, 6};
        break;
    default:
        *out = {wide_lora ? 812.5f : 250.0f, 11, 5};
        break;
    }
    return true;
}

const RegionInfo* getRegion(RegionCode code)
{
    for (const auto& region : kRegions) {
        if (region.code == code) return &region;
    }
    return nullptr;
}

const RegionInfo* getRegionByName(const char* name)
{
    if (!name) return nullptr;
    for (const auto& region : kRegions) {
        if (asciiEqualsIgnoreCase(name, region.name)) return &region;
    }
    return nullptr;
}

bool computeFrequencyPlan(RegionCode region_code,
                          ModemPreset preset,
                          const char* channel_name,
                          uint32_t channel_num,
                          float override_frequency_mhz,
                          float frequency_offset_mhz,
                          FrequencyPlan* out)
{
    if (!out) return false;
    const RegionInfo* region = getRegion(region_code);
    if (!region) return false;
    if (!presetAllowed(*region, preset)) preset = region->default_preset;

    RadioParams params;
    if (!presetParams(preset, region->wide_lora, &params)) return false;

    const char* effective_channel = (channel_name && channel_name[0]) ? channel_name : presetName(preset);
    float freq_slot_width = region->spacing_mhz + (region->padding_mhz * 2.0f) + (params.bandwidth_khz / 1000.0f);
    if (freq_slot_width <= 0.0f) return false;

    float slot_count_f = (region->freq_end_mhz - region->freq_start_mhz + region->spacing_mhz) / freq_slot_width;
    uint32_t slot_count = static_cast<uint32_t>(slot_count_f + 0.5f);
    if (slot_count == 0) return false;

    uint32_t slot = 0;
    float freq = 0.0f;
    if (override_frequency_mhz > 0.0f) {
        freq = override_frequency_mhz;
        slot = 0xFFFFFFFFu;
    } else {
        if (channel_num == 0) {
            if (region->override_slot > 0) {
                slot = static_cast<uint32_t>(region->override_slot - 1);
            } else if (region->override_slot == -1) {
                slot = djb2Hash(presetName(preset)) % slot_count;
            } else {
                slot = djb2Hash(effective_channel) % slot_count;
            }
        } else {
            slot = channel_num - 1;
        }
        slot %= slot_count;
        freq = region->freq_start_mhz + (params.bandwidth_khz / 2000.0f) + region->padding_mhz + (slot * freq_slot_width);
    }

    out->params = params;
    out->frequency_mhz = freq + frequency_offset_mhz;
    out->slot = slot;
    out->slot_count = slot_count;
    out->tx_power_dbm = region->power_limit_dbm ? static_cast<int8_t>(region->power_limit_dbm) : 17;
    return true;
}

uint32_t djb2Hash(const char* text)
{
    uint32_t hash = 5381;
    if (!text) return hash;
    while (*text) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(*text++);
    }
    return hash;
}

uint8_t xorHash(const uint8_t* data, size_t len)
{
    uint8_t out = 0;
    if (!data) return out;
    for (size_t i = 0; i < len; ++i) out ^= data[i];
    return out;
}

bool defaultPsk(uint8_t* out, size_t* out_len)
{
    if (!out_len) return false;
    if (!out) {
        *out_len = sizeof(kDefaultPskBytes);
        return true;
    }
    if (*out_len < sizeof(kDefaultPskBytes)) return false;
    memcpy(out, kDefaultPskBytes, sizeof(kDefaultPskBytes));
    *out_len = sizeof(kDefaultPskBytes);
    return true;
}

bool expandPsk(const uint8_t* psk, size_t psk_len, uint8_t* out, size_t* out_len)
{
    if (!out_len || (!psk && psk_len > 0)) return false;
    size_t cap = out ? *out_len : 0;
    uint8_t expanded[32] = {};
    size_t len = psk_len;

    if (psk_len == 0) {
        len = 0;
    } else if (psk_len == 1) {
        uint8_t psk_index = psk[0];
        if (psk_index == 0) {
            len = 0;
        } else {
            memcpy(expanded, kDefaultPskBytes, sizeof(kDefaultPskBytes));
            expanded[sizeof(kDefaultPskBytes) - 1] =
                static_cast<uint8_t>(expanded[sizeof(kDefaultPskBytes) - 1] + psk_index - 1);
            len = sizeof(kDefaultPskBytes);
        }
    } else if (psk_len < 16) {
        memcpy(expanded, psk, psk_len);
        len = 16;
    } else if (psk_len == 16 || psk_len == 32) {
        memcpy(expanded, psk, psk_len);
        len = psk_len;
    } else if (psk_len < 32) {
        memcpy(expanded, psk, psk_len);
        len = 32;
    } else {
        return false;
    }

    if (!out) {
        *out_len = len;
        return true;
    }
    if (cap < len) return false;
    if (len > 0) memcpy(out, expanded, len);
    *out_len = len;
    return true;
}

bool channelHash(const char* channel_name, const uint8_t* psk, size_t psk_len, uint8_t* out_hash)
{
    if (!channel_name || !out_hash) return false;
    uint8_t expanded[32] = {};
    size_t expanded_len = sizeof(expanded);
    if (!expandPsk(psk, psk_len, expanded, &expanded_len)) return false;
    *out_hash = xorHash(reinterpret_cast<const uint8_t*>(channel_name), strlen(channel_name)) ^
                xorHash(expanded, expanded_len);
    return true;
}

bool cryptoAvailable()
{
    return SLOPOS_MESHTASTIC_HAS_CRYPTO != 0;
}

bool applyAesCtr(uint8_t* bytes, size_t len, const uint8_t* key, size_t key_len, uint32_t from_node, uint64_t packet_id)
{
    if (!bytes || (!key && key_len > 0) || len > kMaxEncryptedPayloadBytes) return false;
    if (key_len == 0) return true;
    if (key_len != 16 && key_len != 32) return false;

#if SLOPOS_MESHTASTIC_HAS_CRYPTO
    uint8_t nonce[16] = {};
    putLe64(nonce, packet_id);
    putLe32(nonce + 8, from_node);

    uint8_t scratch[kMaxEncryptedPayloadBytes] = {};
    if (len > 0) memcpy(scratch, bytes, len);

    if (key_len == 16) {
        CTR<AES128> ctr;
        if (!ctr.setKey(key, key_len)) return false;
        if (!ctr.setIV(nonce, sizeof(nonce))) return false;
        if (!ctr.setCounterSize(4)) return false;
        ctr.encrypt(bytes, scratch, len);
    } else {
        CTR<AES256> ctr;
        if (!ctr.setKey(key, key_len)) return false;
        if (!ctr.setIV(nonce, sizeof(nonce))) return false;
        if (!ctr.setCounterSize(4)) return false;
        ctr.encrypt(bytes, scratch, len);
    }
    return true;
#else
    (void)from_node;
    (void)packet_id;
    return false;
#endif
}

bool encodeEncryptedDataFrame(const PacketHeader& header,
                              const DataPacket& data,
                              const uint8_t* psk,
                              size_t psk_len,
                              PacketFrame* out)
{
    if (!out) return false;
    uint8_t key[32] = {};
    size_t key_len = sizeof(key);
    if (!expandPsk(psk, psk_len, key, &key_len)) return false;

    PacketFrame frame{};
    frame.header = header;
    if (!encodeData(data, frame.payload, sizeof(frame.payload), &frame.payload_len)) return false;
    if (!applyAesCtr(frame.payload, frame.payload_len, key, key_len, header.from, header.id)) return false;
    *out = frame;
    return true;
}

bool decodeEncryptedDataFrame(const PacketFrame& frame,
                              const uint8_t* psk,
                              size_t psk_len,
                              DataPacket* out)
{
    if (!out || frame.payload_len > kMaxEncryptedPayloadBytes) return false;
    uint8_t key[32] = {};
    size_t key_len = sizeof(key);
    if (!expandPsk(psk, psk_len, key, &key_len)) return false;

    uint8_t plain[kMaxEncryptedPayloadBytes] = {};
    if (frame.payload_len > 0) memcpy(plain, frame.payload, frame.payload_len);
    if (!applyAesCtr(plain, frame.payload_len, key, key_len, frame.header.from, frame.header.id)) return false;
    return decodeData(plain, frame.payload_len, out);
}

} // namespace meshtastic
} // namespace slopos
