#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Runtime radio configuration stored in NVS.
// User MUST explicitly configure before radio transmits —
// incorrect settings may be illegal in certain regions.

#include <cstdint>
#include <cstring>

namespace slopos {

enum class ProtocolMode : uint8_t {
    MeshCore = 0,
    Meshtastic = 1,
};

const char* protocolModeName(ProtocolMode mode);
ProtocolMode protocolModeFromByte(uint8_t value);

struct NodePrefs {
    char    node_name[32];
    float   freq;           // MHz (e.g. 869.618)
    float   bw;             // kHz (e.g. 62.5)
    uint8_t sf;             // LoRa spreading factor (6-12)
    uint8_t cr;             // coding rate denominator (5=4/5, 6=4/6, etc.)
    int8_t  tx_power_dbm;   // dBm (2-22)
    bool    configured;     // false until user explicitly saves settings
    uint8_t kbd_backlight;  // 0-255, keyboard backlight brightness
    uint16_t chat_msg_cap;  // Per-channel in-memory message history cap
    ProtocolMode protocol_mode;
    uint8_t meshtastic_region;     // slopos::meshtastic::RegionCode
    uint8_t meshtastic_preset;     // slopos::meshtastic::ModemPreset
    uint8_t meshtastic_hop_limit;
    char    meshtastic_channel[32];
    uint8_t meshtastic_psk[32];
    uint8_t meshtastic_psk_len;

    // Sentinel defaults — radio will NOT transmit until user configures
    void set_defaults() {
        strncpy(node_name, "SlopOS T-Deck", sizeof(node_name) - 1);
        node_name[sizeof(node_name) - 1] = '\0';
        freq = 0.0f;         // 0 = not configured
        bw   = 0.0f;
        sf   = 0;
        cr   = 0;
        tx_power_dbm = 0;
        configured = false;
        kbd_backlight = 127;
        chat_msg_cap = 200;
        protocol_mode = ProtocolMode::MeshCore;
        meshtastic_region = 1; // RegionCode::US
        meshtastic_preset = 1; // ModemPreset::LongFast
        meshtastic_hop_limit = 3;
        strncpy(meshtastic_channel, "LongFast", sizeof(meshtastic_channel) - 1);
        meshtastic_channel[sizeof(meshtastic_channel) - 1] = '\0';
        memset(meshtastic_psk, 0, sizeof(meshtastic_psk));
        meshtastic_psk[0] = 1; // Meshtastic default PSK index
        meshtastic_psk_len = 1;
    }
};

// Load/save from NVS (Preferences)
bool prefs_load(NodePrefs& p);
bool prefs_save(const NodePrefs& p);
bool prefs_exists();

// Expose loaded prefs globally (read-only after boot)
const NodePrefs& prefs_get();
void             prefs_set(const NodePrefs& p);

} // namespace slopos
