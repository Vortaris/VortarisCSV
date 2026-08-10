#pragma once

#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Cell matching used by find / filter. `p_mode` is VCSVTable::MatchMode:
//   0 EXACT, 1 NOCASE_EXACT, 2 CONTAINS, 3 NOCASE_CONTAINS,
//   4 PREFIX, 5 NOCASE_PREFIX.
inline bool cell_matches(const godot::String &p_cell, const godot::String &p_value, int64_t p_mode) {
	switch (p_mode) {
	case 1:
		return p_cell.nocasecmp_to(p_value) == 0;
	case 2:
		return p_cell.contains(p_value);
	case 3:
		return p_cell.containsn(p_value);
	case 4:
		return p_cell.begins_with(p_value);
	case 5:
		return p_cell.to_lower().begins_with(p_value.to_lower());
	default:
		return p_cell == p_value;
	}
}

} // namespace vortariscsv
