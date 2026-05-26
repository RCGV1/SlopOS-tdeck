// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#pragma once

#include "meshtastic_support.h"

namespace slopos {
namespace meshtastic {

class MeshtasticNode {
public:
    static constexpr size_t kMaxChannelName = 32;
    static constexpr size_t kMaxQueuedMessages = 16;
    static constexpr size_t kPacketHistorySize = 64;

    struct Config {
        uint32_t node_num = 0;
        RegionCode region = RegionCode::US;
        ModemPreset preset = ModemPreset::LongFast;
        uint8_t hop_limit = 3;
        char channel_name[kMaxChannelName] = {};
        uint8_t psk[32] = {1};
        size_t psk_len = 1;
    };

    struct Message {
        uint32_t from = 0;
        uint32_t to = 0;
        uint32_t packet_id = 0;
        PortNum portnum = PortNum::Unknown;
        char channel[kMaxChannelName] = {};
        char text[kDataPayloadLen + 1] = {};
        int rssi = 0;
        float snr = 0.0f;
    };

    struct Contact {
        uint32_t node_num = 0;
        char name[32] = {};
        char long_name[40] = {};
        char short_name[5] = {};
        int rssi = 0;
        float snr = 0.0f;
        uint32_t last_seen = 0;
    };

    bool begin(const Config& cfg);
    bool configured() const { return configured_; }
    const Config& config() const { return cfg_; }

    bool getFrequencyPlan(FrequencyPlan* out) const;
    bool buildTextFrame(const char* text, uint32_t packet_id, PacketFrame* out) const;
    bool buildTextFrameTo(uint32_t to, const char* text, uint32_t packet_id, PacketFrame* out) const;
    bool buildTextBytes(const char* text, uint32_t packet_id, uint8_t* out, size_t out_len, size_t* written) const;
    bool buildTextBytesTo(uint32_t to, const char* text, uint32_t packet_id, uint8_t* out, size_t out_len, size_t* written) const;
    bool buildDataFrame(PortNum portnum,
                        const uint8_t* payload,
                        size_t payload_len,
                        uint32_t to,
                        uint32_t packet_id,
                        bool want_response,
                        bool want_ack,
                        PacketFrame* out) const;
    bool buildDataBytes(PortNum portnum,
                        const uint8_t* payload,
                        size_t payload_len,
                        uint32_t to,
                        uint32_t packet_id,
                        bool want_response,
                        bool want_ack,
                        uint8_t* out,
                        size_t out_len,
                        size_t* written) const;

    bool ingestFrame(const PacketFrame& frame, int rssi, float snr, uint32_t now = 0);
    bool ingestBytes(const uint8_t* bytes, size_t len, int rssi, float snr, uint32_t now = 0);
    int pollMessages(Message* out, int max);
    int pendingMessageCount() const { return queue_count_; }
    int exportContacts(Contact* out, int max) const;
    bool nodeNumForContact(const char* name, uint32_t* out) const;
    const char* nameForNode(uint32_t node_num, char* fallback, size_t fallback_len) const;
    void clear();

private:
    struct SeenPacket {
        uint32_t from = 0;
        uint32_t id = 0;
        bool used = false;
    };

    bool resolveChannelHash(uint8_t* out_hash) const;
    bool hasSeen(uint32_t from, uint32_t id) const;
    void rememberSeen(uint32_t from, uint32_t id);
    bool queueMessage(const PacketFrame& frame, const DataPacket& data, int rssi, float snr);
    bool handleDecodedData(const PacketFrame& frame, const DataPacket& data, int rssi, float snr, uint32_t now);
    void updateContact(uint32_t node_num,
                       const char* long_name,
                       const char* short_name,
                       int rssi,
                       float snr,
                       uint32_t now);
    int findContactByNode(uint32_t node_num) const;

    Config cfg_;
    bool configured_ = false;
    uint8_t channel_hash_ = 0;

    SeenPacket seen_[kPacketHistorySize] = {};
    size_t seen_next_ = 0;

    Message queue_[kMaxQueuedMessages] = {};
    int queue_head_ = 0;
    int queue_tail_ = 0;
    int queue_count_ = 0;

    Contact contacts_[32] = {};
    int contact_count_ = 0;
};

} // namespace meshtastic
} // namespace slopos
