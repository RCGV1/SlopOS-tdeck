// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Mock prefs implementation for native test environment.
// Provides stub implementations so keyboard.cpp (which includes prefs.h)
// can compile and link without real NVS (Preferences) hardware.

#include "hal/prefs.h"

namespace slopos {

static NodePrefs g_prefs;
static bool g_prefs_initialized = false;

static void ensure_defaults() {
    if (!g_prefs_initialized) {
        g_prefs.set_defaults();
        g_prefs_initialized = true;
    }
}

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
    ensure_defaults();
    p = g_prefs;
    return true;
}

bool prefs_save(const NodePrefs& p) {
    ensure_defaults();
    g_prefs = p;
    return true;
}

bool prefs_exists() {
    return true;
}

const NodePrefs& prefs_get() {
    ensure_defaults();
    return g_prefs;
}

void prefs_set(const NodePrefs& p) {
    ensure_defaults();
    g_prefs = p;
}

} // namespace slopos
