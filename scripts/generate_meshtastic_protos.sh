#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTO_ROOT="${1:-/tmp/meshtastic-protobufs}"
OUT_DIR="$ROOT/src/meshtastic/generated"

if [[ ! -d "$PROTO_ROOT/meshtastic" ]]; then
  echo "Meshtastic protobuf checkout not found: $PROTO_ROOT" >&2
  echo "Clone https://github.com/meshtastic/protobufs there, or pass its path as argv[1]." >&2
  exit 1
fi

PLUGIN="$(python3 - <<'PY'
import nanopb
import os
print(os.path.join(os.path.dirname(nanopb.__file__), "generator", "protoc-gen-nanopb"))
PY
)"

NANOPB_PROTO_DIR="$(python3 - <<'PY'
import nanopb
import os
print(os.path.join(os.path.dirname(nanopb.__file__), "generator", "proto"))
PY
)"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

PROTO_FILES=()
while IFS= read -r proto_file; do
  PROTO_FILES+=("$proto_file")
done < <(find "$PROTO_ROOT/meshtastic" -name '*.proto' | sort)

protoc \
  --plugin="protoc-gen-nanopb=$PLUGIN" \
  --proto_path="$PROTO_ROOT" \
  --proto_path="$NANOPB_PROTO_DIR" \
  --nanopb_out="-S.cpp -I$PROTO_ROOT:$OUT_DIR" \
  "${PROTO_FILES[@]}"

PROTO_COMMIT="$(git -C "$PROTO_ROOT" log -1 --format='%h %s' 2>/dev/null || echo "unknown")"

cat > "$OUT_DIR/README.md" <<EOF
# Generated Meshtastic Protobufs

This directory is generated from the official Meshtastic protobuf schema with nanopb.

Source checkout used for this branch:

- Repository: \`https://github.com/meshtastic/protobufs\`
- Commit: \`$PROTO_COMMIT\`
- Generator/runtime: \`nanopb-0.4.9.1\`

Regenerate from a local protobuf checkout:

\`\`\`bash
scripts/generate_meshtastic_protos.sh /path/to/meshtastic-protobufs
\`\`\`

Do not hand-edit the \`.pb.h\` or \`.pb.cpp\` files.
EOF
