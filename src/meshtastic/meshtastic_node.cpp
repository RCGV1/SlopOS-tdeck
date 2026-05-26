// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "meshtastic_node.h"

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
    if (!configured_ || !text || !out || packet_id == 0) return false;

    DataPacket data;
    if (!makeTextData(text, &data)) return false;

    PacketHeader header;
    header.to = kBroadcastNode;
    header.from = cfg_.node_num;
    header.id = packet_id;
    header.flags = makeFlags(cfg_.hop_limit);
    header.channel = channel_hash_;

    return encodeEncryptedDataFrame(header, data, cfg_.psk, cfg_.psk_len, out);
}

bool MeshtasticNode::buildTextBytes(const char* text, uint32_t packet_id, uint8_t* out, size_t out_len, size_t* written) const
{
    if (!out || !written) return false;
    PacketFrame frame;
    if (!buildTextFrame(text, packet_id, &frame)) return false;
    return encodeFrame(frame, out, out_len, written);
}

bool MeshtasticNode::ingestFrame(const PacketFrame& frame, int rssi, float snr)
{
    if (!configured_) return false;
    if (frame.payload_len > kMaxEncryptedPayloadBytes) return false;
    if (frame.header.from == 0 || frame.header.from == cfg_.node_num) return false;
    if (frame.header.to != kBroadcastNode && frame.header.to != cfg_.node_num) return false;
    if (frame.header.channel != channel_hash_) return false;
    if (hasSeen(frame.header.from, frame.header.id)) return false;

    DataPacket data;
    if (!decodeEncryptedDataFrame(frame, cfg_.psk, cfg_.psk_len, &data)) return false;
    if (!queueMessage(frame, data, rssi, snr)) return false;
    rememberSeen(frame.header.from, frame.header.id);
    return true;
}

bool MeshtasticNode::ingestBytes(const uint8_t* bytes, size_t len, int rssi, float snr)
{
    PacketFrame frame;
    if (!decodeFrame(bytes, len, &frame)) return false;
    return ingestFrame(frame, rssi, snr);
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

} // namespace meshtastic
} // namespace slopos
