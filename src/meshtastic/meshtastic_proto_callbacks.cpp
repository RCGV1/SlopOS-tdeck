// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "meshtastic/deviceonly.pb.h"

#include <pb_decode.h>
#include <pb_encode.h>

bool meshtastic_NodeDatabase_callback(pb_istream_t* istream,
                                      pb_ostream_t* ostream,
                                      const pb_field_t* field)
{
    const auto* iter = reinterpret_cast<const pb_field_iter_t*>(field);
    if (!iter || iter->tag != meshtastic_NodeDatabase_nodes_tag) return false;

    if (ostream) {
        const auto* nodes =
            static_cast<const std::vector<meshtastic_NodeInfoLite>*>(iter->pData);
        for (const auto& node : *nodes) {
            if (!pb_encode_tag_for_field(ostream, iter)) return false;
            if (!pb_encode_submessage(ostream, meshtastic_NodeInfoLite_fields, &node)) return false;
        }
    }

    if (istream && istream->bytes_left) {
        meshtastic_NodeInfoLite node = meshtastic_NodeInfoLite_init_zero;
        auto* nodes = static_cast<std::vector<meshtastic_NodeInfoLite>*>(iter->pData);
        if (!pb_decode(istream, meshtastic_NodeInfoLite_fields, &node)) return false;
        nodes->push_back(node);
    }

    return true;
}
