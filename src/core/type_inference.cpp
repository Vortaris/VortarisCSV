#include "type_inference.h"

#include <algorithm>
#include <vector>

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string.hpp>

#include "type_converter.h"

namespace vortariscsv {

using godot::Dictionary;
using godot::PackedStringArray;
using godot::PropertyInfo;
using godot::String;
using godot::Variant;

namespace {

bool is_no_value(const String &p_cell, const String &p_null_token) {
	if (p_cell.is_empty()) {
		return true;
	}
	return !p_null_token.is_empty() && p_cell == p_null_token;
}

// Detects the element type of an array cell's single part (no nested arrays).
String detect_scalar(const String &p_part, const InferOptions &p_opts) {
	String s = p_part.strip_edges();
	if (p_opts.detect_booleans) {
		String lower = s.to_lower();
		if (lower == "true" || lower == "false") {
			return "bool";
		}
	}
	if (s.is_valid_int()) {
		return "int";
	}
	if (s.is_valid_float()) {
		return "float";
	}
	return "string";
}

// Parses a comma-separated numeric list; returns the count or -1.
int64_t numeric_component_count(const String &p_cell) {
	String s = p_cell.strip_edges();
	int64_t open = s.find("(");
	if (open != -1) {
		int64_t close = s.rfind(")");
		if (close > open) {
			s = s.substr(open + 1, close - open - 1);
		}
	}
	PackedStringArray parts = s.split(",", false);
	int64_t count = 0;
	for (int64_t i = 0; i < parts.size(); i++) {
		if (!parts[i].strip_edges().is_valid_float()) {
			return -1;
		}
		count++;
	}
	return count;
}

} // namespace

bool split_header_type(const String &p_header, const String &p_separator,
		String &r_name, String &r_type) {
	if (p_separator.is_empty()) {
		return false;
	}
	const int64_t idx = p_header.find(p_separator);
	if (idx <= 0) {
		return false; // no separator, or the name part is empty
	}
	r_name = p_header.substr(0, idx).strip_edges();
	r_type = p_header.substr(idx + p_separator.length()).strip_edges();
	return !r_name.is_empty();
}

bool is_valid_canonical_type_name(const String &p_type_name) {
	const String t = p_type_name.strip_edges();
	if (t.is_empty() || t == "json") {
		return t == "json";
	}
	PropertyInfo pi;
	if (!property_for_type_name(t, pi)) {
		return false;
	}
	// property_for_type_name accepts anything get_type_by_name doesn't reject,
	// but get_type_by_name returns Variant::VARIANT_MAX (not NIL) for unknown
	// names — reject that sentinel.
	return pi.type != Variant::VARIANT_MAX;
}

String detect_cell_type(const String &p_cell, const InferOptions &p_opts) {
	if (is_no_value(p_cell, p_opts.null_token)) {
		return String();
	}
	String s = p_cell.strip_edges();

	// JSON object / array literal.
	if (p_opts.detect_json && (s.begins_with("{") || s.begins_with("["))) {
		if (godot::JSON::parse_string(s).get_type() != godot::Variant::NIL) {
			return "json";
		}
	}

	// Hex color.
	if (s.begins_with("#")) {
		return "Color";
	}

	// Array via sub-delimiter.
	if (!p_opts.array_delimiter.is_empty() && p_opts.array_delimiter != "," && s.find(p_opts.array_delimiter) != -1) {
		PackedStringArray parts = s.split(p_opts.array_delimiter);
		String elem = String();
		bool mixed = false;
		for (int64_t i = 0; i < parts.size(); i++) {
			String t = detect_scalar(parts[i], p_opts);
			if (t.is_empty()) {
				continue;
			}
			if (elem.is_empty()) {
				elem = t;
			} else if (elem != t) {
				if ((elem == "int" && t == "float") || (elem == "float" && t == "int")) {
					elem = "float";
				} else {
					mixed = true;
					break;
				}
			}
		}
		if (mixed) {
			return "string[]";
		}
		if (elem.is_empty()) {
			return "string[]";
		}
		return elem + "[]";
	}

	// Comma-separated numeric → Vector2/3/4 (or Rect2 at 4 components).
	if (p_opts.detect_vectors && s.find(",") != -1) {
		int64_t count = numeric_component_count(s);
		switch (count) {
		case 2:
			return "Vector2";
		case 3:
			return "Vector3";
		case 4:
			return "Vector4";
		default:
			break;
		}
	}

	return detect_scalar(s, p_opts);
}

Dictionary infer_column_types(const PackedStringArray &p_headers,
		const std::vector<PackedStringArray> &p_rows, const InferOptions &p_opts) {
	Dictionary out;
	const int64_t col_count = p_headers.is_empty() ? 0 : p_headers.size();

	for (int64_t col = 0; col < col_count; col++) {
		// An explicit header schema ("hp:int") declares the column type and wins
		// over cell-based inference; invalid type names fall back to string.
		if (!p_opts.header_type_separator.is_empty()) {
			String name, type;
			if (split_header_type(p_headers[col], p_opts.header_type_separator, name, type)) {
				out[p_headers[col]] = is_valid_canonical_type_name(type) ? type : "string";
				continue;
			}
		}

		std::vector<String> seen;
		for (const PackedStringArray &row : p_rows) {
			if (col >= row.size()) {
				continue;
			}
			String t = detect_cell_type(row[col], p_opts);
			if (!t.is_empty() && std::find(seen.begin(), seen.end(), t) == seen.end()) {
				seen.push_back(t);
			}
		}

		String unified;
		// Any plain string forces the whole column to string.
		bool has_string = false;
		bool has_bool = false;
		bool has_int = false;
		bool has_float = false;
		bool has_json = false;
		std::vector<String> vector_types;
		std::vector<String> array_types;
		for (const String &t : seen) {
			if (t == "string") {
				has_string = true;
			} else if (t == "bool") {
				has_bool = true;
			} else if (t == "int") {
				has_int = true;
			} else if (t == "float") {
				has_float = true;
			} else if (t == "json") {
				has_json = true;
			} else if (t.ends_with("[]")) {
				array_types.push_back(t);
			} else {
				vector_types.push_back(t);
			}
		}

		if (seen.empty()) {
			unified = "string";
		} else if (has_string) {
			unified = "string";
		} else if (has_json) {
			unified = "json";
		} else if (has_bool && (has_int || has_float)) {
			unified = "string"; // bool mixed with numbers is ambiguous.
		} else if (has_bool) {
			unified = "bool";
		} else if (has_float || has_int) {
			unified = has_float ? "float" : "int";
		} else if (!array_types.empty()) {
			String first = array_types[0];
			bool mixed = false;
			for (const String &a : array_types) {
				if (a != first) {
					if ((a == "int[]" && first == "float[]") || (a == "float[]" && first == "int[]")) {
						first = "float[]";
					} else {
						mixed = true;
						break;
					}
				}
			}
			unified = mixed ? "string[]" : first;
		} else if (!vector_types.empty()) {
			String first = vector_types[0];
			bool same = true;
			for (const String &v : vector_types) {
				if (v != first) {
					same = false;
					break;
				}
			}
			unified = same ? first : "string";
		} else {
			unified = "string";
		}

		out[p_headers[col]] = unified;
	}

	return out;
}

} // namespace vortariscsv
