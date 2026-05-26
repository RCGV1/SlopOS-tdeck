# Generated Meshtastic Protobufs

This directory is generated from the official Meshtastic protobuf schema with nanopb.

Source checkout used for this branch:

- Repository: `https://github.com/meshtastic/protobufs`
- Commit: `59cb394 Add T_ECHO_CARD identifier to mesh.proto (#919)`
- Generator/runtime: `nanopb-0.4.9.1`

Regenerate from a local protobuf checkout:

```bash
scripts/generate_meshtastic_protos.sh /path/to/meshtastic-protobufs
```

Do not hand-edit the `.pb.h` or `.pb.cpp` files.
