#pragma once

#include <vector>

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Options controlling heuristic column-type detection.
struct InferOptions {
	// Sub-list delimiter used to recognise array cells ("1;2;3" -> int[]).
	godot::String array_delimiter = ";";
	// Cells equal to this token carry no type information.
	godot::String null_token;
	// Recognise "true"/"false" (case-insensitive) as bool.
	bool detect_booleans = false;
	// Recognise comma-separated numeric cells as Vector2/3/4.
	bool detect_vectors = true;
	// Recognise "{...}" / "[...]" cells as JSON.
	bool detect_json = true;
	// When non-empty, a header of the form "name<sep>Type" (e.g. "hp:int")
	// declares an explicit column type that wins over cell-based inference.
	// Invalid type names fall back to "string". Default ":".
	godot::String header_type_separator = ":";
};

// Canonical type name of a single cell: "bool", "int", "float", "string",
// "Vector2", "Vector3", "Vector4", "Rect2", "Color", "int[]", "float[]",
// "bool[]", "string[]", "json". Empty / null cells return an empty string.
godot::String detect_cell_type(const godot::String &p_cell, const InferOptions &p_opts);

// Splits `p_header` at the first occurrence of `p_separator`. Returns false when
// the header has no such separator. On true, `r_name` is the part before the
// separator (trimmed) and `r_type` the part after (trimmed).
bool split_header_type(const godot::String &p_header, const godot::String &p_separator,
		godot::String &r_name, godot::String &r_type);

// Whether `p_type_name` is a canonical type name usable as an explicit header
// schema type ("int", "float", "Vector2", "int[]", "json", ...).
bool is_valid_canonical_type_name(const godot::String &p_type_name);

// Infers a canonical type name per column (header name -> type name), unifying
// the per-cell detections (INT+FLOAT -> FLOAT, any STRING wins, etc.). When
// `p_opts.header_type_separator` is non-empty, headers carrying an explicit
// type annotation ("hp:int") use that type directly.
godot::Dictionary infer_column_types(const godot::PackedStringArray &p_headers,
		const std::vector<godot::PackedStringArray> &p_rows, const InferOptions &p_opts);

} // namespace vortariscsv
