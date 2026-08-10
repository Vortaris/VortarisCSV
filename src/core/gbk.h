#pragma once

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "gbk_table.h"

namespace vortariscsv {

// Decodes a GBK/GB2312 byte buffer into a Godot String. ASCII passes through;
// two-byte sequences in the 0xA1-0xF7 lead range are looked up in the embedded
// table (covers the GB2312 core and common GBK characters). Bytes that are
// neither ASCII nor a known lead/trail pair become U+FFFD.
godot::String gbk_bytes_to_string(const uint8_t *p_data, int64_t p_len);

// Convenience wrapper over a PackedByteArray.
godot::String gbk_bytes_to_string(const godot::PackedByteArray &p_bytes);

} // namespace vortariscsv
