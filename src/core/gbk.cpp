#include "gbk.h"

#include <vector>

namespace vortariscsv {

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

} // namespace vortariscsv
