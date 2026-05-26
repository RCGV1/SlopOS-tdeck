// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "meshtastic_support.h"

#include <cstring>

#include <AES.h>
#include <CTR.h>
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

static bool putByte(uint8_t* out, size_t out_len, size_t* pos, uint8_t value)
{
    if (!out || !pos || *pos >= out_len) return false;
    out[(*pos)++] = value;
    return true;
}

static bool putBytes(uint8_t* out, size_t out_len, size_t* pos, const uint8_t* data, size_t len)
{
    if (!out || !pos || (!data && len > 0) || *pos > out_len || len > out_len - *pos) return false;
    if (len > 0) memcpy(out + *pos, data, len);
    *pos += len;
    return true;
}

static bool encodeVarint(uint64_t value, uint8_t* out, size_t out_len, size_t* pos)
{
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value) byte |= 0x80;
        if (!putByte(out, out_len, pos, byte)) return false;
    } while (value);
    return true;
}

static bool decodeVarint(const uint8_t* data, size_t len, size_t* pos, uint64_t* value)
{
    if (!data || !pos || !value) return false;
    uint64_t result = 0;
    uint8_t shift = 0;
    while (*pos < len && shift < 64) {
        uint8_t byte = data[(*pos)++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            *value = result;
            return true;
        }
        shift += 7;
    }
    return false;
}

static bool encodeTag(uint8_t field, uint8_t wire_type, uint8_t* out, size_t out_len, size_t* pos)
{
    return encodeVarint((static_cast<uint64_t>(field) << 3) | wire_type, out, out_len, pos);
}

static bool encodeFixed32Field(uint8_t field, uint32_t value, uint8_t* out, size_t out_len, size_t* pos)
{
    uint8_t buf[4];
    if (!encodeTag(field, 5, out, out_len, pos)) return false;
    putLe32(buf, value);
    return putBytes(out, out_len, pos, buf, sizeof(buf));
}

static bool skipField(uint8_t wire_type, const uint8_t* data, size_t len, size_t* pos)
{
    uint64_t ignored = 0;
    switch (wire_type) {
    case 0:
        return decodeVarint(data, len, pos, &ignored);
    case 1:
        if (*pos > len || len - *pos < 8) return false;
        *pos += 8;
        return true;
    case 2:
        if (!decodeVarint(data, len, pos, &ignored)) return false;
        if (ignored > len - *pos) return false;
        *pos += static_cast<size_t>(ignored);
        return true;
    case 5:
        if (*pos > len || len - *pos < 4) return false;
        *pos += 4;
        return true;
    default:
        return false;
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
    if (!out || !written || data.payload_len > kDataPayloadLen) return false;
    size_t pos = 0;
    if (!encodeTag(1, 0, out, out_len, &pos)) return false;
    if (!encodeVarint(static_cast<uint16_t>(data.portnum), out, out_len, &pos)) return false;

    if (data.payload_len > 0) {
        if (!encodeTag(2, 2, out, out_len, &pos)) return false;
        if (!encodeVarint(data.payload_len, out, out_len, &pos)) return false;
        if (!putBytes(out, out_len, &pos, data.payload, data.payload_len)) return false;
    }
    if (data.want_response) {
        if (!encodeTag(3, 0, out, out_len, &pos)) return false;
        if (!encodeVarint(1, out, out_len, &pos)) return false;
    }
    if (data.dest && !encodeFixed32Field(4, data.dest, out, out_len, &pos)) return false;
    if (data.source && !encodeFixed32Field(5, data.source, out, out_len, &pos)) return false;
    if (data.request_id && !encodeFixed32Field(6, data.request_id, out, out_len, &pos)) return false;
    if (data.reply_id && !encodeFixed32Field(7, data.reply_id, out, out_len, &pos)) return false;
    if (data.emoji && !encodeFixed32Field(8, data.emoji, out, out_len, &pos)) return false;
    if (data.has_bitfield) {
        if (!encodeTag(9, 0, out, out_len, &pos)) return false;
        if (!encodeVarint(data.bitfield, out, out_len, &pos)) return false;
    }

    *written = pos;
    return true;
}

bool decodeData(const uint8_t* data, size_t len, DataPacket* out)
{
    if (!data || !out) return false;
    *out = DataPacket{};
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag = 0;
        if (!decodeVarint(data, len, &pos, &tag)) return false;
        uint8_t field = static_cast<uint8_t>(tag >> 3);
        uint8_t wire = static_cast<uint8_t>(tag & 0x07);
        uint64_t value = 0;

        switch (field) {
        case 1:
            if (wire != 0 || !decodeVarint(data, len, &pos, &value)) return false;
            if (value > static_cast<uint16_t>(PortNum::Max)) return false;
            out->portnum = static_cast<PortNum>(value);
            break;
        case 2:
            if (wire != 2 || !decodeVarint(data, len, &pos, &value)) return false;
            if (value > kDataPayloadLen || value > len - pos) return false;
            out->payload_len = static_cast<size_t>(value);
            if (out->payload_len > 0) memcpy(out->payload, data + pos, out->payload_len);
            pos += out->payload_len;
            break;
        case 3:
            if (wire != 0 || !decodeVarint(data, len, &pos, &value)) return false;
            out->want_response = value != 0;
            break;
        case 4:
            if (wire != 5 || pos > len || len - pos < 4) return false;
            out->dest = getLe32(data + pos);
            pos += 4;
            break;
        case 5:
            if (wire != 5 || pos > len || len - pos < 4) return false;
            out->source = getLe32(data + pos);
            pos += 4;
            break;
        case 6:
            if (wire != 5 || pos > len || len - pos < 4) return false;
            out->request_id = getLe32(data + pos);
            pos += 4;
            break;
        case 7:
            if (wire != 5 || pos > len || len - pos < 4) return false;
            out->reply_id = getLe32(data + pos);
            pos += 4;
            break;
        case 8:
            if (wire != 5 || pos > len || len - pos < 4) return false;
            out->emoji = getLe32(data + pos);
            pos += 4;
            break;
        case 9:
            if (wire != 0 || !decodeVarint(data, len, &pos, &value)) return false;
            out->has_bitfield = true;
            out->bitfield = static_cast<uint32_t>(value);
            break;
        default:
            if (!skipField(wire, data, len, &pos)) return false;
            break;
        }
    }
    return true;
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
