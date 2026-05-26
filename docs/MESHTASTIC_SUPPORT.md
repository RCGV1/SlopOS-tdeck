# Meshtastic Support

SlopOS now includes a tested Meshtastic protocol support layer under `src/meshtastic/`. It is compiled into the T-Deck firmware and covered by native GoogleTest tests.

## Implemented

- **LoRa frame wire format** — 16-byte Meshtastic packet header, 255-byte LoRa frame cap, hop-limit/ack/MQTT flag helpers.
- **Official protobuf bindings** — generated nanopb C++ bindings from the upstream Meshtastic protobuf schema for admin, apponly, ATAK, canned messages, channel, client/local/device/interdevice messages, config, connection status, device UI, mesh, module config, MQTT, paxcount, portnums, powermon, remote hardware, RTTTL, serial, store-forward, telemetry, and xmodem.
- **Data and MeshPacket helpers** — conversion between SlopOS frame structs and generated `meshtastic_Data` / `meshtastic_MeshPacket`, plus generic nanopb encode/decode helpers for other generated message types.
- **Port, modem, and region constants** — wrapper enum values are derived from the generated upstream protobuf constants.
- **Frequency planning** — modem preset parameters, regional frequency ranges, channel slot calculation, power limits, and override frequency support.
- **Channel compatibility** — Meshtastic channel hash calculation, default PSK alias expansion, empty/no-encryption PSK handling, 16-byte and 32-byte PSKs.
- **AES-CTR payload crypto** — Meshtastic nonce layout using packet ID and sender node number, with Arduino Crypto AES-128/AES-256 CTR.
- **Text node helper** — `MeshtasticNode` can build encrypted broadcast text frames, encode them to bytes, ingest frames/bytes, reject wrong-channel/self/duplicate frames, and queue decoded text messages with RSSI/SNR metadata.

## API Surface

| Source | Purpose |
|--------|---------|
| `src/meshtastic/generated/meshtastic/*.pb.h/.pb.cpp` | Official Meshtastic protobuf bindings generated with nanopb |
| `src/meshtastic/generated/README.md` | Protobuf source commit and regeneration notes |
| `src/meshtastic/meshtastic_support.h` | Constants, enums, packet structs, wire encode/decode, frequency planning, PSK/hash/crypto helpers |
| `src/meshtastic/meshtastic_support.cpp` | Meshtastic protocol implementation |
| `src/meshtastic/meshtastic_proto_callbacks.cpp` | Callback support needed by the generated NodeDatabase schema |
| `src/meshtastic/meshtastic_node.h` | Small configured node facade for text-frame send/receive |
| `src/meshtastic/meshtastic_node.cpp` | Node facade implementation and message queue |
| `scripts/generate_meshtastic_protos.sh` | Rebuilds generated bindings from a local `meshtastic/protobufs` checkout |
| `test/test_meshtastic_support/` | Native tests for wire format, generated protobufs, crypto, regions, and node behavior |

## Current Boundary

This branch adds a protobuf-backed Meshtastic protocol and text-node layer, and verifies it with native tests plus an ESP32 firmware build. The generated schema covers Meshtastic message types beyond text, and the generic helpers can encode/decode them. It does not yet replace the existing MeshCore runtime in the UI, expose Meshtastic channel setup in Settings, implement persistent NodeDB/router behavior, or include hardware interoperability results from a real Meshtastic node pair.

## Protobuf Source

The generated bindings in `src/meshtastic/generated/` were produced from `https://github.com/meshtastic/protobufs` commit `59cb394 Add T_ECHO_CARD identifier to mesh.proto (#919)` with `nanopb-0.4.9.1`.

Regenerate them with:

```bash
scripts/generate_meshtastic_protos.sh /path/to/meshtastic-protobufs
```

## Verification

```bash
pio test -e native_test -f test_meshtastic_support -v
pio test -e native_test -v
pio run -e SlopOS_TDeck
```

The dedicated Meshtastic suite covers 23 cases, including official wire limits from generated constants, enum value checks against upstream protobufs, header layout, `Data` and `MeshPacket` nanopb round trips, generic `Position` encode/decode, oversized payload rejection, regional frequency plans, default channel hash behavior, AES-CTR encrypted frame round trips, node byte ingest, duplicate rejection, corrupt-frame retry behavior, and frequency-plan exposure.
