#include "gbk.h"

#include <cstring>
#include <unordered_map>
#include <vector>

namespace vortariscsv {

using godot::PackedByteArray;
using godot::String;

String gbk_bytes_to_string(const uint8_t *p_data, int64_t p_len) {
	std::vector<char32_t> out;
	out.reserve((size_t)(p_len + 1));

	int64_t i = 0;
	while (i < p_len) {
		const uint8_t b = p_data[i];
		if (b < 0x80) {
			out.push_back((char32_t)b); // ASCII / Latin-1 single byte.
			i++;
			continue;
		}
		if (i + 1 < p_len) {
			const uint32_t cp = gbk_lookup(b, p_data[i + 1]);
			if (cp != 0) {
				out.push_back((char32_t)cp);
				i += 2;
				continue;
			}
		}
		// Unknown or truncated lead byte: replacement character.
		out.push_back(0xFFFD);
		i++;
	}

	out.push_back(0);
	return String(out.data());
}

String gbk_bytes_to_string(const godot::PackedByteArray &p_bytes) {
	return gbk_bytes_to_string(p_bytes.ptr(), p_bytes.size());
}

namespace {
// Reverse table: Unicode code point -> GBK (lead, trail) packed in a uint16_t.
// Lazily built once from k_gbk_table; duplicates keep the first byte pair.
const std::unordered_map<char32_t, uint16_t> &gbk_reverse_table() {
	static std::unordered_map<char32_t, uint16_t> reverse = [] {
		std::unordered_map<char32_t, uint16_t> m;
		m.reserve(24000);
		for (uint32_t lead = 0x81; lead <= 0xFE; lead++) {
			for (uint32_t ti = 0; ti < 190; ti++) {
				const uint16_t cp = k_gbk_table[(lead - 0x81) * 190 + ti];
				if (cp == 0) {
					continue;
				}
				const uint32_t trail = (ti < 63) ? ti + 0x40 : ti + 0x41;
				const char32_t key = (char32_t)cp;
				if (m.find(key) == m.end()) {
					m[key] = (uint16_t)((lead << 8) | trail);
				}
			}
		}
		return m;
	}();
	return reverse;
}
} // namespace

PackedByteArray gbk_string_to_bytes(const String &p_text) {
	const std::unordered_map<char32_t, uint16_t> &reverse = gbk_reverse_table();
	// Accumulate in a plain byte buffer first — repeated PackedByteArray
	// element writes go through COW ptrw() each time (slow and, on some
	// builds, unreliable mid-resize); one bulk copy at the end is both safer
	// and faster.
	std::vector<uint8_t> buf;
	buf.reserve((size_t)p_text.length() + 16);
	for (int64_t i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (c < 0x80) {
			buf.push_back((uint8_t)c);
			continue;
		}
		const auto it = reverse.find(c);
		if (it != reverse.end()) {
			buf.push_back((uint8_t)(it->second >> 8));
			buf.push_back((uint8_t)(it->second & 0xFF));
		} else {
			// Unmappable code point: ASCII placeholder keeps the file loadable.
			buf.push_back('?');
		}
	}
	PackedByteArray out;
	out.resize((int64_t)buf.size());
	if (!buf.empty()) {
		memcpy(out.ptrw(), buf.data(), buf.size());
	}
	return out;
}

} // namespace vortariscsv
