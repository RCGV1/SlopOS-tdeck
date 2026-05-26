# Meshtastic Support

SlopOS now includes a tested Meshtastic protocol support layer under `src/meshtastic/`. It is compiled into the T-Deck firmware and covered by native GoogleTest tests.

## Implemented

- **LoRa frame wire format** — 16-byte Meshtastic packet header, 255-byte LoRa frame cap, hop-limit/ack/MQTT flag helpers.
- **Data protobuf subset** — `Data` encode/decode for port number, payload, response flag, fixed32 source/destination/request/reply IDs, emoji, and bitfield.
- **Port, modem, and region constants** — values match the upstream Meshtastic protobuf and firmware tables for the supported enums.
- **Frequency planning** — modem preset parameters, regional frequency ranges, channel slot calculation, power limits, and override frequency support.
- **Channel compatibility** — Meshtastic channel hash calculation, default PSK alias expansion, empty/no-encryption PSK handling, 16-byte and 32-byte PSKs.
- **AES-CTR payload crypto** — Meshtastic nonce layout using packet ID and sender node number, with Arduino Crypto AES-128/AES-256 CTR.
- **Text node helper** — `MeshtasticNode` can build encrypted broadcast text frames, encode them to bytes, ingest frames/bytes, reject wrong-channel/self/duplicate frames, and queue decoded text messages with RSSI/SNR metadata.

## API Surface

| Source | Purpose |
|--------|---------|
| `src/meshtastic/meshtastic_support.h` | Constants, enums, packet structs, wire encode/decode, frequency planning, PSK/hash/crypto helpers |
| `src/meshtastic/meshtastic_support.cpp` | Meshtastic protocol implementation |
| `src/meshtastic/meshtastic_node.h` | Small configured node facade for text-frame send/receive |
| `src/meshtastic/meshtastic_node.cpp` | Node facade implementation and message queue |
| `test/test_meshtastic_support/` | Native tests for wire format, protobuf fields, crypto, regions, and node behavior |

## Current Boundary

This branch adds the Meshtastic-compatible protocol and text-node layer, and verifies it with native tests plus an ESP32 firmware build. It does not yet replace the existing MeshCore runtime in the UI, expose Meshtastic channel setup in Settings, implement NodeDB/admin/position/telemetry/router behavior, or include hardware interoperability results from a real Meshtastic node pair.

## Verification

```bash
pio test -e native_test -f test_meshtastic_support -v
pio test -e native_test -v
pio run -e SlopOS_TDeck
```

The dedicated Meshtastic suite covers 18 cases, including official wire limits, header layout, `Data` protobuf round trips, oversized payload rejection, regional frequency plans, default channel hash behavior, AES-CTR encrypted frame round trips, node byte ingest, duplicate rejection, corrupt-frame retry behavior, and frequency-plan exposure.
