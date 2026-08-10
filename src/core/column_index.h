#pragma once

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Maps header names to column indices. Rebuilt on demand; supports optional
// case-insensitive lookup (first match wins on duplicate names).
class ColumnIndex {
public:
	ColumnIndex() = default;
	ColumnIndex(const godot::PackedStringArray &p_headers, bool p_case_insensitive) {
		rebuild(p_headers, p_case_insensitive);
	}

	void rebuild(const godot::PackedStringArray &p_headers, bool p_case_insensitive);
	bool empty() const { return headers_.is_empty(); }
	int64_t find(const godot::String &p_name) const;
	int64_t size() const { return headers_.size(); }

private:
	godot::PackedStringArray headers_;
	bool case_insensitive_ = false;
	godot::HashMap<godot::String, int64_t> map_; // lowercased name -> index
};

} // namespace vortariscsv
