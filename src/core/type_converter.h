#pragma once

#include <functional>

#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace vortariscsv {

// Context that tunes how cells are converted to typed values.
struct ConvertContext {
	// Sub-list delimiter inside a cell for array types (e.g. "1;2;3" -> int[]).
	godot::String array_delimiter = ";";
	// Cells equal to this token are treated as "no value" (caller keeps the
	// target's default). Empty string cells are always treated this way.
	godot::String null_token;
	// Optional resolver for OBJECT-typed cells (foreign keys). Filled by the
	// reflection binder; empty in the raw core (OBJECT cells then fail cleanly).
	std::function<godot::Variant(const godot::String &p_cell)> object_resolver;
};

// Parses `p_cell` into a Variant matching `p_prop` (Variant::Type + hint +
// hint_string). Returns:
//   - a valid Variant on success;
//   - NIL with empty r_err when the cell is empty or equals null_token
//     ("no value" — caller should keep the target's default);
//   - NIL with a non-empty r_err on a parse failure.
godot::Variant parse_to_type(const godot::String &p_cell, const godot::PropertyInfo &p_prop,
		const ConvertContext &p_ctx, godot::String &r_err);

// Convenience: same as above with a bare Variant::Type (no hint). Useful for
// inference-driven conversion where the target type is known only as a type id.
godot::Variant parse_to_type(const godot::String &p_cell, godot::Variant::Type p_type,
		const ConvertContext &p_ctx, godot::String &r_err);

} // namespace vortariscsv
