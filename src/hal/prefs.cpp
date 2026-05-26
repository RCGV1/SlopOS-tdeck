// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include "prefs.h"
#include <Preferences.h>

namespace slopos {

static constexpr const char* NVS_NS = "slopos";
static NodePrefs g_prefs;

const char* protocolModeName(ProtocolMode mode) {
    switch (mode) {
    case ProtocolMode::Meshtastic:
        return "Meshtastic";
    case ProtocolMode::MeshCore:
    default:
        return "MeshCore";
    }
}

ProtocolMode protocolModeFromByte(uint8_t value) {
    return value == static_cast<uint8_t>(ProtocolMode::Meshtastic)
        ? ProtocolMode::Meshtastic
        : ProtocolMode::MeshCore;
}

bool prefs_load(NodePrefs& p) {
    Preferences nvs;
    if (!nvs.begin(NVS_NS, true)) return false;

    nvs.getString("name", p.node_name, sizeof(p.node_name));
    p.freq         = nvs.getFloat("freq", 0.0f);
    p.bw           = nvs.getFloat("bw", 0.0f);
    p.sf           = nvs.getUChar("sf", 0);
    p.cr           = nvs.getUChar("cr", 0);
    p.tx_power_dbm  = nvs.getChar("txpwr", 0);
    if (p.tx_power_dbm > 0 && p.tx_power_dbm < 2) p.tx_power_dbm = 2;
    if (p.tx_power_dbm > 22) p.tx_power_dbm = 22;
    p.configured    = nvs.getBool("cfg", false);
    p.kbd_backlight = nvs.getUChar("kbd_bl", 127);
    p.chat_msg_cap  = nvs.getUShort("chat_cap", 200);
    p.protocol_mode = protocolModeFromByte(
        nvs.getUChar("proto", static_cast<uint8_t>(p.protocol_mode)));
    p.meshtastic_region = nvs.getUChar("mt_region", p.meshtastic_region);
    p.meshtastic_preset = nvs.getUChar("mt_preset", p.meshtastic_preset);
    p.meshtastic_hop_limit = nvs.getUChar("mt_hops", p.meshtastic_hop_limit);
    if (p.meshtastic_hop_limit > 7) p.meshtastic_hop_limit = 3;
    nvs.getString("mt_ch", p.meshtastic_channel, sizeof(p.meshtastic_channel));
    if (p.meshtastic_channel[0] == '\0') {
        strncpy(p.meshtastic_channel, "LongFast", sizeof(p.meshtastic_channel) - 1);
        p.meshtastic_channel[sizeof(p.meshtastic_channel) - 1] = '\0';
    }
    size_t psk_len = nvs.getBytesLength("mt_psk");
    if (psk_len > 0 && psk_len <= sizeof(p.meshtastic_psk)) {
        p.meshtastic_psk_len = static_cast<uint8_t>(
            nvs.getBytes("mt_psk", p.meshtastic_psk, sizeof(p.meshtastic_psk)));
    }
    if (p.meshtastic_psk_len > sizeof(p.meshtastic_psk)) {
        memset(p.meshtastic_psk, 0, sizeof(p.meshtastic_psk));
        p.meshtastic_psk[0] = 1;
        p.meshtastic_psk_len = 1;
    }

    nvs.end();
    return true;
}

bool prefs_save(const NodePrefs& p) {
    Preferences nvs;
    if (!nvs.begin(NVS_NS, false)) return false;

    nvs.putString("name", p.node_name);
    nvs.putFloat("freq", p.freq);
    nvs.putFloat("bw", p.bw);
    nvs.putUChar("sf", p.sf);
    nvs.putUChar("cr", p.cr);
    nvs.putChar("txpwr", p.tx_power_dbm < 2 ? (int8_t)2 : (p.tx_power_dbm > 22 ? (int8_t)22 : p.tx_power_dbm));
    nvs.putBool("cfg", p.configured);
    nvs.putUChar("kbd_bl", p.kbd_backlight);
    nvs.putUShort("chat_cap", p.chat_msg_cap);
    nvs.putUChar("proto", static_cast<uint8_t>(p.protocol_mode));
    nvs.putUChar("mt_region", p.meshtastic_region);
    nvs.putUChar("mt_preset", p.meshtastic_preset);
    nvs.putUChar("mt_hops", p.meshtastic_hop_limit > 7 ? (uint8_t)3 : p.meshtastic_hop_limit);
    nvs.putString("mt_ch", p.meshtastic_channel);
    uint8_t psk_len = p.meshtastic_psk_len;
    if (psk_len > sizeof(p.meshtastic_psk)) psk_len = 1;
    nvs.putBytes("mt_psk", p.meshtastic_psk, psk_len);

    nvs.end();
    return true;
}

bool prefs_exists() {
    Preferences nvs;
    if (!nvs.begin(NVS_NS, true)) return false;
    bool exists = nvs.isKey("cfg");
    nvs.end();
    return exists;
}

const NodePrefs& prefs_get() {
    static bool loaded = false;
    if (!loaded) {
        g_prefs.set_defaults();
        prefs_load(g_prefs);
        loaded = true;
    }
    return g_prefs;
}

void prefs_set(const NodePrefs& p) {
    g_prefs = p;
    prefs_save(p);
}

} // namespace slopos
