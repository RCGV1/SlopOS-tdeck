// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "meshtastic_node.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace slopos {
namespace meshtastic {

bool MeshtasticNode::begin(const Config& cfg)
{
    if (cfg.node_num == 0 || cfg.node_num == kBroadcastNode) return false;
    if (cfg.psk_len > sizeof(cfg.psk)) return false;
    if (!getRegion(cfg.region)) return false;

    cfg_ = cfg;
    if (cfg_.channel_name[0] == '\0') {
        strncpy(cfg_.channel_name, presetName(cfg_.preset), sizeof(cfg_.channel_name) - 1);
        cfg_.channel_name[sizeof(cfg_.channel_name) - 1] = '\0';
    }
    cfg_.hop_limit &= kPacketFlagsHopLimitMask;

    if (!resolveChannelHash(&channel_hash_)) {
        configured_ = false;
        return false;
    }
    clear();
    configured_ = true;
    return true;
}

bool MeshtasticNode::getFrequencyPlan(FrequencyPlan* out) const
{
    if (!configured_) return false;
    return computeFrequencyPlan(cfg_.region, cfg_.preset, cfg_.channel_name, 0, 0.0f, 0.0f, out);
}

bool MeshtasticNode::buildTextFrame(const char* text, uint32_t packet_id, PacketFrame* out) const
{
    return buildTextFrameTo(kBroadcastNode, text, packet_id, out);
}

bool MeshtasticNode::buildTextFrameTo(uint32_t to, const char* text, uint32_t packet_id, PacketFrame* out) const
{
    if (!configured_ || !text || !out || packet_id == 0) return false;

    DataPacket data;
    if (!makeTextData(text, &data)) return false;
    data.source = cfg_.node_num;
    data.dest = to;

    PacketHeader header;
    header.to = to;
    header.from = cfg_.node_num;
    header.id = packet_id;
    header.flags = makeFlags(cfg_.hop_limit, to != kBroadcastNode);
    header.channel = channel_hash_;

    return encodeEncryptedDataFrame(header, data, cfg_.psk, cfg_.psk_len, out);
}

bool MeshtasticNode::buildTextBytes(const char* text, uint32_t packet_id, uint8_t* out, size_t out_len, size_t* written) const
{
    return buildTextBytesTo(kBroadcastNode, text, packet_id, out, out_len, written);
}

bool MeshtasticNode::buildTextBytesTo(uint32_t to, const char* text, uint32_t packet_id, uint8_t* out, size_t out_len, size_t* written) const
{
    if (!out || !written) return false;
    PacketFrame frame;
    if (!buildTextFrameTo(to, text, packet_id, &frame)) return false;
    return encodeFrame(frame, out, out_len, written);
}

bool MeshtasticNode::buildDataFrame(PortNum portnum,
                                    const uint8_t* payload,
                                    size_t payload_len,
                                    uint32_t to,
                                    uint32_t packet_id,
                                    bool want_response,
                                    bool want_ack,
                                    PacketFrame* out) const
{
    if (!configured_ || !out || packet_id == 0 || payload_len > kDataPayloadLen) return false;
    if (!payload && payload_len > 0) return false;

    DataPacket data;
    data.portnum = portnum;
    data.source = cfg_.node_num;
    data.dest = to;
    data.want_response = want_response;
    data.payload_len = payload_len;
    if (payload_len > 0) memcpy(data.payload, payload, payload_len);

    PacketHeader header;
    header.to = to;
    header.from = cfg_.node_num;
    header.id = packet_id;
    header.flags = makeFlags(cfg_.hop_limit, want_ack);
    header.channel = channel_hash_;

    return encodeEncryptedDataFrame(header, data, cfg_.psk, cfg_.psk_len, out);
}

bool MeshtasticNode::buildDataBytes(PortNum portnum,
                                    const uint8_t* payload,
                                    size_t payload_len,
                                    uint32_t to,
                                    uint32_t packet_id,
                                    bool want_response,
                                    bool want_ack,
                                    uint8_t* out,
                                    size_t out_len,
                                    size_t* written) const
{
    if (!out || !written) return false;
    PacketFrame frame;
    if (!buildDataFrame(portnum, payload, payload_len, to, packet_id, want_response, want_ack, &frame)) {
        return false;
    }
    return encodeFrame(frame, out, out_len, written);
}

bool MeshtasticNode::ingestFrame(const PacketFrame& frame, int rssi, float snr, uint32_t now)
{
    if (!configured_) return false;
    if (frame.payload_len > kMaxEncryptedPayloadBytes) return false;
    if (frame.header.from == 0 || frame.header.from == cfg_.node_num) return false;
    if (frame.header.to != kBroadcastNode && frame.header.to != cfg_.node_num) return false;
    if (frame.header.channel != channel_hash_) return false;
    if (hasSeen(frame.header.from, frame.header.id)) return false;

    DataPacket data;
    if (!decodeEncryptedDataFrame(frame, cfg_.psk, cfg_.psk_len, &data)) return false;
    if (!handleDecodedData(frame, data, rssi, snr, now)) return false;
    rememberSeen(frame.header.from, frame.header.id);
    return true;
}

bool MeshtasticNode::ingestBytes(const uint8_t* bytes, size_t len, int rssi, float snr, uint32_t now)
{
    PacketFrame frame;
    if (!decodeFrame(bytes, len, &frame)) return false;
    return ingestFrame(frame, rssi, snr, now);
}

int MeshtasticNode::pollMessages(Message* out, int max)
{
    if (!out || max <= 0) return 0;
    int n = 0;
    while (n < max && queue_count_ > 0) {
        out[n++] = queue_[queue_tail_];
        queue_tail_ = (queue_tail_ + 1) % static_cast<int>(kMaxQueuedMessages);
        queue_count_--;
    }
    return n;
}

int MeshtasticNode::exportContacts(Contact* out, int max) const
{
    if (!out || max <= 0) return 0;
    int n = contact_count_ < max ? contact_count_ : max;
    for (int i = 0; i < n; ++i) out[i] = contacts_[i];
    return n;
}

bool MeshtasticNode::nodeNumForContact(const char* name, uint32_t* out) const
{
    if (!name || !out) return false;
    for (int i = 0; i < contact_count_; ++i) {
        const Contact& c = contacts_[i];
        if ((c.name[0] && strcmp(c.name, name) == 0) ||
            (c.long_name[0] && strcmp(c.long_name, name) == 0) ||
            (c.short_name[0] && strcmp(c.short_name, name) == 0)) {
            *out = c.node_num;
            return true;
        }
    }
    if (name[0] == '!') {
        char* end = nullptr;
        uint32_t parsed = static_cast<uint32_t>(strtoul(name + 1, &end, 16));
        if (end && *end == '\0' && parsed != 0 && parsed != kBroadcastNode) {
            *out = parsed;
            return true;
        }
    }
    return false;
}

const char* MeshtasticNode::nameForNode(uint32_t node_num, char* fallback, size_t fallback_len) const
{
    for (int i = 0; i < contact_count_; ++i) {
        if (contacts_[i].node_num == node_num && contacts_[i].name[0]) {
            return contacts_[i].name;
        }
    }
    if (!fallback || fallback_len == 0) return "";
    snprintf(fallback, fallback_len, "!%08lX", static_cast<unsigned long>(node_num));
    fallback[fallback_len - 1] = '\0';
    return fallback;
}

void MeshtasticNode::clear()
{
    for (auto& seen : seen_) {
        seen = SeenPacket{};
    }
    seen_next_ = 0;
    for (auto& msg : queue_) {
        msg = Message{};
    }
    queue_head_ = 0;
    queue_tail_ = 0;
    queue_count_ = 0;
    for (auto& contact : contacts_) {
        contact = Contact{};
    }
    contact_count_ = 0;
}

bool MeshtasticNode::resolveChannelHash(uint8_t* out_hash) const
{
    if (!out_hash) return false;
    return channelHash(cfg_.channel_name, cfg_.psk, cfg_.psk_len, out_hash);
}

bool MeshtasticNode::hasSeen(uint32_t from, uint32_t id) const
{
    for (const auto& seen : seen_) {
        if (seen.used && seen.from == from && seen.id == id) {
            return true;
        }
    }
    return false;
}

void MeshtasticNode::rememberSeen(uint32_t from, uint32_t id)
{
    seen_[seen_next_].from = from;
    seen_[seen_next_].id = id;
    seen_[seen_next_].used = true;
    seen_next_ = (seen_next_ + 1) % kPacketHistorySize;
}

bool MeshtasticNode::queueMessage(const PacketFrame& frame, const DataPacket& data, int rssi, float snr)
{
    if (data.portnum != PortNum::TextMessage && data.portnum != PortNum::TextMessageCompressed) {
        return false;
    }
    if (queue_count_ >= static_cast<int>(kMaxQueuedMessages)) {
        return false;
    }

    Message& msg = queue_[queue_head_];
    msg = Message{};
    msg.from = frame.header.from;
    msg.to = frame.header.to;
    msg.packet_id = frame.header.id;
    msg.portnum = data.portnum;
    msg.rssi = rssi;
    msg.snr = snr;
    strncpy(msg.channel, cfg_.channel_name, sizeof(msg.channel) - 1);
    if (!extractText(data, msg.text, sizeof(msg.text))) {
        msg = Message{};
        return false;
    }

    queue_head_ = (queue_head_ + 1) % static_cast<int>(kMaxQueuedMessages);
    queue_count_++;
    return true;
}

bool MeshtasticNode::handleDecodedData(const PacketFrame& frame, const DataPacket& data, int rssi, float snr, uint32_t now)
{
    updateContact(frame.header.from, nullptr, nullptr, rssi, snr, now);

    if (data.portnum == PortNum::TextMessage || data.portnum == PortNum::TextMessageCompressed) {
        return queueMessage(frame, data, rssi, snr);
    }

    if (data.portnum == PortNum::NodeInfo) {
        meshtastic_User user = meshtastic_User_init_zero;
        if (!decodeProtoMessage(&meshtastic_User_msg, data.payload, data.payload_len, &user)) return false;
        updateContact(frame.header.from, user.long_name, user.short_name, rssi, snr, now);
        return true;
    }

    if (data.portnum == PortNum::Position) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        if (!decodeProtoMessage(&meshtastic_Position_msg, data.payload, data.payload_len, &pos)) return false;
        return true;
    }

    if (data.portnum == PortNum::Routing || data.portnum == PortNum::TraceRoute ||
        data.portnum == PortNum::Telemetry || data.portnum == PortNum::NeighborInfo) {
        return true;
    }

    return false;
}

void MeshtasticNode::updateContact(uint32_t node_num,
                                   const char* long_name,
                                   const char* short_name,
                                   int rssi,
                                   float snr,
                                   uint32_t now)
{
    if (node_num == 0 || node_num == cfg_.node_num || node_num == kBroadcastNode) return;

    int idx = findContactByNode(node_num);
    if (idx < 0) {
        if (contact_count_ < static_cast<int>(sizeof(contacts_) / sizeof(contacts_[0]))) {
            idx = contact_count_++;
        } else {
            idx = 0;
            for (int i = 1; i < contact_count_; ++i) {
                if (contacts_[i].last_seen < contacts_[idx].last_seen) idx = i;
            }
        }
        contacts_[idx] = Contact{};
        contacts_[idx].node_num = node_num;
        snprintf(contacts_[idx].name, sizeof(contacts_[idx].name), "!%08lX", static_cast<unsigned long>(node_num));
    }

    Contact& contact = contacts_[idx];
    contact.rssi = rssi;
    contact.snr = snr;
    if (now != 0) contact.last_seen = now;

    if (long_name && long_name[0]) {
        strncpy(contact.long_name, long_name, sizeof(contact.long_name) - 1);
        contact.long_name[sizeof(contact.long_name) - 1] = '\0';
    }
    if (short_name && short_name[0]) {
        strncpy(contact.short_name, short_name, sizeof(contact.short_name) - 1);
        contact.short_name[sizeof(contact.short_name) - 1] = '\0';
    }
    if (contact.long_name[0]) {
        strncpy(contact.name, contact.long_name, sizeof(contact.name) - 1);
    } else if (contact.short_name[0]) {
        strncpy(contact.name, contact.short_name, sizeof(contact.name) - 1);
    }
    contact.name[sizeof(contact.name) - 1] = '\0';
}

int MeshtasticNode::findContactByNode(uint32_t node_num) const
{
    for (int i = 0; i < contact_count_; ++i) {
        if (contacts_[i].node_num == node_num) return i;
    }
    return -1;
}

} // namespace meshtastic
} // namespace slopos
