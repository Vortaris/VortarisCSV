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
};

// Canonical type name of a single cell: "bool", "int", "float", "string",
// "Vector2", "Vector3", "Vector4", "Rect2", "Color", "int[]", "float[]",
// "bool[]", "string[]", "json". Empty / null cells return an empty string.
godot::String detect_cell_type(const godot::String &p_cell, const InferOptions &p_opts);

// Infers a canonical type name per column (header name -> type name), unifying
// the per-cell detections (INT+FLOAT -> FLOAT, any STRING wins, etc.).
godot::Dictionary infer_column_types(const godot::PackedStringArray &p_headers,
		const std::vector<godot::PackedStringArray> &p_rows, const InferOptions &p_opts);

} // namespace vortariscsv
