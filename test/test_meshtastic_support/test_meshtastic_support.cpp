// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <gtest/gtest.h>

#include "meshtastic/meshtastic_node.h"
#include "meshtastic/meshtastic_support.h"

#include <cstring>

namespace {

using namespace slopos::meshtastic;

TEST(MeshtasticSupportTest, ConstantsMatchOfficialWireLimits) {
    EXPECT_EQ(kHeaderLength, 16u);
    EXPECT_EQ(kMaxLoRaFrameBytes, 255u);
    EXPECT_EQ(kMaxEncryptedPayloadBytes, 239u);
    EXPECT_EQ(kDataPayloadLen, 233u);
    EXPECT_EQ(kBroadcastNode, 0xFFFFFFFFu);
}

TEST(MeshtasticSupportTest, EnumValuesMatchOfficialProtoNumbers) {
    EXPECT_EQ(static_cast<uint16_t>(PortNum::TextMessage), 1u);
    EXPECT_EQ(static_cast<uint16_t>(PortNum::NodeInfo), 4u);
    EXPECT_EQ(static_cast<uint16_t>(PortNum::Telemetry), 67u);
    EXPECT_EQ(static_cast<uint16_t>(PortNum::AtakPluginV2), 78u);
    EXPECT_EQ(static_cast<uint8_t>(RegionCode::EU868), 3u);
    EXPECT_EQ(static_cast<uint8_t>(RegionCode::EU866), 29u);
    EXPECT_EQ(static_cast<uint8_t>(RegionCode::EUN868), 32u);
}

TEST(MeshtasticSupportTest, FlagsRoundTripIndividualBits) {
    uint8_t flags = makeFlags(5, true, true, 6);
    EXPECT_EQ(getHopLimit(flags), 5u);
    EXPECT_EQ(getHopStart(flags), 6u);
    EXPECT_TRUE(getWantAck(flags));
    EXPECT_TRUE(getViaMqtt(flags));
    EXPECT_EQ(flags, 0xDDu);
}

TEST(MeshtasticSupportTest, HeaderEncodesLittleEndianWireLayout) {
    PacketHeader header;
    header.to = 0xFFFFFFFFu;
    header.from = 0x12345678u;
    header.id = 0xAABBCCDDu;
    header.flags = makeFlags(3, true, false, 2);
    header.channel = 0x08;
    header.next_hop = 0x44;
    header.relay_node = 0x55;

    uint8_t bytes[kHeaderLength] = {};
    ASSERT_TRUE(encodeHeader(header, bytes, sizeof(bytes)));

    const uint8_t expected[kHeaderLength] = {
        0xff, 0xff, 0xff, 0xff,
        0x78, 0x56, 0x34, 0x12,
        0xdd, 0xcc, 0xbb, 0xaa,
        0x4b, 0x08, 0x44, 0x55,
    };
    EXPECT_EQ(memcmp(bytes, expected, sizeof(expected)), 0);

    PacketHeader decoded;
    ASSERT_TRUE(decodeHeader(bytes, sizeof(bytes), &decoded));
    EXPECT_EQ(decoded.to, header.to);
    EXPECT_EQ(decoded.from, header.from);
    EXPECT_EQ(decoded.id, header.id);
    EXPECT_EQ(decoded.flags, header.flags);
    EXPECT_EQ(decoded.channel, header.channel);
    EXPECT_EQ(decoded.next_hop, header.next_hop);
    EXPECT_EQ(decoded.relay_node, header.relay_node);
}

TEST(MeshtasticSupportTest, DataEncodesTextMessageAsProtoSubset) {
    DataPacket data;
    ASSERT_TRUE(makeTextData("hello", &data));

    uint8_t encoded[32] = {};
    size_t written = 0;
    ASSERT_TRUE(encodeData(data, encoded, sizeof(encoded), &written));

    const uint8_t expected[] = {0x08, 0x01, 0x12, 0x05, 'h', 'e', 'l', 'l', 'o'};
    ASSERT_EQ(written, sizeof(expected));
    EXPECT_EQ(memcmp(encoded, expected, sizeof(expected)), 0);

    DataPacket decoded;
    ASSERT_TRUE(decodeData(encoded, written, &decoded));
    EXPECT_EQ(decoded.portnum, PortNum::TextMessage);
    EXPECT_EQ(decoded.payload_len, 5u);

    char text[8] = {};
    ASSERT_TRUE(extractText(decoded, text, sizeof(text)));
    EXPECT_STREQ(text, "hello");
}

TEST(MeshtasticSupportTest, DataRoundTripsControlFields) {
    DataPacket data;
    ASSERT_TRUE(makeTextData("ack", &data));
    data.want_response = true;
    data.dest = 0x01020304u;
    data.source = 0xA0B0C0D0u;
    data.request_id = 0x10203040u;
    data.reply_id = 0x11223344u;
    data.emoji = 0x0001F44Du;
    data.has_bitfield = true;
    data.bitfield = 0x24u;

    uint8_t encoded[96] = {};
    size_t written = 0;
    ASSERT_TRUE(encodeData(data, encoded, sizeof(encoded), &written));

    DataPacket decoded;
    ASSERT_TRUE(decodeData(encoded, written, &decoded));
    EXPECT_EQ(decoded.portnum, PortNum::TextMessage);
    EXPECT_TRUE(decoded.want_response);
    EXPECT_EQ(decoded.dest, data.dest);
    EXPECT_EQ(decoded.source, data.source);
    EXPECT_EQ(decoded.request_id, data.request_id);
    EXPECT_EQ(decoded.reply_id, data.reply_id);
    EXPECT_EQ(decoded.emoji, data.emoji);
    EXPECT_TRUE(decoded.has_bitfield);
    EXPECT_EQ(decoded.bitfield, data.bitfield);
}

TEST(MeshtasticSupportTest, RejectsOversizedTextPayload) {
    char text[kDataPayloadLen + 2] = {};
    memset(text, 'x', sizeof(text) - 1);

    DataPacket data;
    EXPECT_FALSE(makeTextData(text, &data));
}

TEST(MeshtasticSupportTest, FrameRoundTripsHeaderAndPayload) {
    PacketFrame frame;
    frame.header.from = 0x12345678u;
    frame.header.id = 7;
    frame.header.flags = makeFlags(3);
    frame.payload[0] = 0x08;
    frame.payload[1] = 0x01;
    frame.payload_len = 2;

    uint8_t encoded[kMaxLoRaFrameBytes] = {};
    size_t written = 0;
    ASSERT_TRUE(encodeFrame(frame, encoded, sizeof(encoded), &written));
    EXPECT_EQ(written, kHeaderLength + 2);

    PacketFrame decoded;
    ASSERT_TRUE(decodeFrame(encoded, written, &decoded));
    EXPECT_EQ(decoded.header.from, frame.header.from);
    EXPECT_EQ(decoded.header.id, frame.header.id);
    EXPECT_EQ(decoded.payload_len, frame.payload_len);
    EXPECT_EQ(decoded.payload[0], 0x08);
    EXPECT_EQ(decoded.payload[1], 0x01);
}

TEST(MeshtasticSupportTest, PresetParamsMatchLongFastAndNarrowSlow) {
    RadioParams params;
    ASSERT_TRUE(presetParams(ModemPreset::LongFast, false, &params));
    EXPECT_FLOAT_EQ(params.bandwidth_khz, 250.0f);
    EXPECT_EQ(params.spreading_factor, 11u);
    EXPECT_EQ(params.coding_rate, 5u);

    ASSERT_TRUE(presetParams(ModemPreset::NarrowSlow, false, &params));
    EXPECT_FLOAT_EQ(params.bandwidth_khz, 62.5f);
    EXPECT_EQ(params.spreading_factor, 8u);
    EXPECT_EQ(params.coding_rate, 6u);
}

TEST(MeshtasticSupportTest, RegionLookupIncludesOfficialTDeckBands) {
    const RegionInfo* us = getRegionByName("us");
    ASSERT_NE(us, nullptr);
    EXPECT_EQ(us->code, RegionCode::US);
    EXPECT_FLOAT_EQ(us->freq_start_mhz, 902.0f);
    EXPECT_FLOAT_EQ(us->freq_end_mhz, 928.0f);

    const RegionInfo* eu = getRegion(RegionCode::EU868);
    ASSERT_NE(eu, nullptr);
    EXPECT_STREQ(eu->name, "EU_868");
    EXPECT_EQ(eu->power_limit_dbm, 27u);
}

TEST(MeshtasticSupportTest, FrequencyPlanMatchesOfficialSlotFormula) {
    FrequencyPlan plan;
    ASSERT_TRUE(computeFrequencyPlan(RegionCode::US, ModemPreset::LongFast, "", 0, 0.0f, 0.0f, &plan));
    EXPECT_EQ(plan.slot_count, 104u);
    EXPECT_EQ(plan.slot, 19u);
    EXPECT_NEAR(plan.frequency_mhz, 906.875f, 0.0001f);
    EXPECT_EQ(plan.tx_power_dbm, 30);

    ASSERT_TRUE(computeFrequencyPlan(RegionCode::EU868, ModemPreset::LongFast, "", 0, 0.0f, 0.0f, &plan));
    EXPECT_EQ(plan.slot_count, 1u);
    EXPECT_NEAR(plan.frequency_mhz, 869.525f, 0.0001f);

    ASSERT_TRUE(computeFrequencyPlan(RegionCode::EUN868, ModemPreset::NarrowSlow, "", 0, 0.0f, 0.0f, &plan));
    EXPECT_EQ(plan.slot_count, 3u);
    EXPECT_EQ(plan.slot, 0u);
    EXPECT_NEAR(plan.frequency_mhz, 869.44165f, 0.0001f);
}

TEST(MeshtasticSupportTest, ChannelHashExpandsDefaultPskAlias) {
    uint8_t psk_alias = 1;
    uint8_t hash = 0;
    ASSERT_TRUE(channelHash("LongFast", &psk_alias, 1, &hash));
    EXPECT_EQ(hash, 0x08u);

    uint8_t psk[32] = {};
    size_t psk_len = sizeof(psk);
    ASSERT_TRUE(expandPsk(&psk_alias, 1, psk, &psk_len));
    ASSERT_EQ(psk_len, 16u);
    EXPECT_EQ(psk[0], 0xd4u);
    EXPECT_EQ(psk[15], 0x01u);
}

TEST(MeshtasticSupportTest, AesCtrEncryptDecryptRoundTripUsesMeshtasticNonce) {
    ASSERT_TRUE(cryptoAvailable());
    uint8_t psk_alias = 1;
    DataPacket data;
    ASSERT_TRUE(makeTextData("encrypted hello", &data));

    PacketHeader header;
    header.to = kBroadcastNode;
    header.from = 0x12345678u;
    header.id = 0xAABBCCDDu;
    header.flags = makeFlags(3);
    header.channel = 0x08;

    PacketFrame encrypted;
    ASSERT_TRUE(encodeEncryptedDataFrame(header, data, &psk_alias, 1, &encrypted));

    uint8_t plain[64] = {};
    size_t plain_len = 0;
    ASSERT_TRUE(encodeData(data, plain, sizeof(plain), &plain_len));
    ASSERT_EQ(encrypted.payload_len, plain_len);
    EXPECT_NE(memcmp(encrypted.payload, plain, plain_len), 0);

    DataPacket decoded;
    ASSERT_TRUE(decodeEncryptedDataFrame(encrypted, &psk_alias, 1, &decoded));

    char text[32] = {};
    ASSERT_TRUE(extractText(decoded, text, sizeof(text)));
    EXPECT_STREQ(text, "encrypted hello");
}

TEST(MeshtasticSupportTest, NodeBuildsEncryptedTextFrameForConfiguredChannel) {
    MeshtasticNode node;
    MeshtasticNode::Config cfg;
    cfg.node_num = 0x12345678u;
    cfg.region = RegionCode::US;
    cfg.preset = ModemPreset::LongFast;
    cfg.hop_limit = 3;
    ASSERT_TRUE(node.begin(cfg));

    PacketFrame frame;
    ASSERT_TRUE(node.buildTextFrame("mesh hello", 0x01020304u, &frame));
    EXPECT_EQ(frame.header.to, kBroadcastNode);
    EXPECT_EQ(frame.header.from, cfg.node_num);
    EXPECT_EQ(frame.header.id, 0x01020304u);
    EXPECT_EQ(getHopLimit(frame.header.flags), 3u);
    EXPECT_EQ(frame.header.channel, 0x08u);
    EXPECT_GT(frame.payload_len, 0u);

    DataPacket decoded;
    ASSERT_TRUE(decodeEncryptedDataFrame(frame, cfg.psk, cfg.psk_len, &decoded));
    char text[32] = {};
    ASSERT_TRUE(extractText(decoded, text, sizeof(text)));
    EXPECT_STREQ(text, "mesh hello");
}

TEST(MeshtasticSupportTest, NodeEncodesAndIngestsBytes) {
    MeshtasticNode sender;
    MeshtasticNode::Config sender_cfg;
    sender_cfg.node_num = 0x11111111u;
    ASSERT_TRUE(sender.begin(sender_cfg));

    MeshtasticNode receiver;
    MeshtasticNode::Config receiver_cfg;
    receiver_cfg.node_num = 0x22222222u;
    ASSERT_TRUE(receiver.begin(receiver_cfg));

    uint8_t bytes[kMaxLoRaFrameBytes] = {};
    size_t written = 0;
    ASSERT_TRUE(sender.buildTextBytes("over lora", 0x42u, bytes, sizeof(bytes), &written));
    ASSERT_TRUE(receiver.ingestBytes(bytes, written, -72, 9.5f));
    EXPECT_EQ(receiver.pendingMessageCount(), 1);

    MeshtasticNode::Message msg;
    ASSERT_EQ(receiver.pollMessages(&msg, 1), 1);
    EXPECT_EQ(msg.from, sender_cfg.node_num);
    EXPECT_EQ(msg.to, kBroadcastNode);
    EXPECT_EQ(msg.packet_id, 0x42u);
    EXPECT_EQ(msg.portnum, PortNum::TextMessage);
    EXPECT_STREQ(msg.channel, "LongFast");
    EXPECT_STREQ(msg.text, "over lora");
    EXPECT_EQ(msg.rssi, -72);
    EXPECT_FLOAT_EQ(msg.snr, 9.5f);
}

TEST(MeshtasticSupportTest, NodeRejectsDuplicateOrWrongChannelFrames) {
    MeshtasticNode sender;
    MeshtasticNode::Config sender_cfg;
    sender_cfg.node_num = 0x11111111u;
    ASSERT_TRUE(sender.begin(sender_cfg));

    MeshtasticNode receiver;
    MeshtasticNode::Config receiver_cfg;
    receiver_cfg.node_num = 0x22222222u;
    ASSERT_TRUE(receiver.begin(receiver_cfg));

    PacketFrame frame;
    ASSERT_TRUE(sender.buildTextFrame("once", 0x99u, &frame));
    ASSERT_TRUE(receiver.ingestFrame(frame, -80, 3.0f));
    EXPECT_FALSE(receiver.ingestFrame(frame, -80, 3.0f));
    EXPECT_EQ(receiver.pendingMessageCount(), 1);

    PacketFrame wrong_channel = frame;
    wrong_channel.header.id = 0x100u;
    wrong_channel.header.channel ^= 0x01u;
    EXPECT_FALSE(receiver.ingestFrame(wrong_channel, -80, 3.0f));
    EXPECT_EQ(receiver.pendingMessageCount(), 1);
}

TEST(MeshtasticSupportTest, CorruptFrameDoesNotPoisonDuplicateHistory) {
    MeshtasticNode sender;
    MeshtasticNode::Config sender_cfg;
    sender_cfg.node_num = 0x11111111u;
    ASSERT_TRUE(sender.begin(sender_cfg));

    MeshtasticNode receiver;
    MeshtasticNode::Config receiver_cfg;
    receiver_cfg.node_num = 0x22222222u;
    ASSERT_TRUE(receiver.begin(receiver_cfg));

    PacketFrame frame;
    ASSERT_TRUE(sender.buildTextFrame("retry after noise", 0x123u, &frame));

    PacketFrame corrupt = frame;
    ASSERT_GT(corrupt.payload_len, 0u);
    corrupt.payload[0] ^= 0xFFu;
    EXPECT_FALSE(receiver.ingestFrame(corrupt, -90, 1.0f));
    EXPECT_EQ(receiver.pendingMessageCount(), 0);

    EXPECT_TRUE(receiver.ingestFrame(frame, -70, 8.0f));
    EXPECT_EQ(receiver.pendingMessageCount(), 1);
}

TEST(MeshtasticSupportTest, NodeExposesFrequencyPlan) {
    MeshtasticNode node;
    MeshtasticNode::Config cfg;
    cfg.node_num = 0x12345678u;
    cfg.region = RegionCode::EU868;
    cfg.preset = ModemPreset::LongFast;
    ASSERT_TRUE(node.begin(cfg));

    FrequencyPlan plan;
    ASSERT_TRUE(node.getFrequencyPlan(&plan));
    EXPECT_NEAR(plan.frequency_mhz, 869.525f, 0.0001f);
    EXPECT_EQ(plan.params.spreading_factor, 11u);
    EXPECT_EQ(plan.tx_power_dbm, 27);
}

} // namespace
