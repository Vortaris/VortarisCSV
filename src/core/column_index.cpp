#include "column_index.h"

namespace vortariscsv {

using godot::PackedStringArray;
using godot::String;

void ColumnIndex::rebuild(const PackedStringArray &p_headers, bool p_case_insensitive) {
	headers_ = p_headers;
	case_insensitive_ = p_case_insensitive;
	map_.clear();
	for (int64_t i = 0; i < headers_.size(); i++) {
		String key = headers_[i];
		if (case_insensitive_) {
			key = key.to_lower();
		}
		if (!map_.has(key)) {
			map_[key] = i;
		}
	}
}

int64_t ColumnIndex::find(const String &p_name) const {
	String key = case_insensitive_ ? p_name.to_lower() : p_name;
	if (map_.has(key)) {
		return map_[key];
	}
	return -1;
}

} // namespace vortariscsv
