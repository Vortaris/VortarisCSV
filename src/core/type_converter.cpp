#include "type_converter.h"

#include <vector>

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>

namespace vortariscsv {

using godot::Array;
using godot::PROPERTY_HINT_ARRAY_TYPE;
using godot::PackedStringArray;
using godot::PropertyInfo;
using godot::String;
using godot::StringName;
using godot::Variant;

namespace {

bool is_no_value(const String &p_cell, const String &p_null_token) {
	if (p_cell.is_empty()) {
		return true;
	}
	return !p_null_token.is_empty() && p_cell == p_null_token;
}

// Splits a cell into sub-parts, preferring the array_delimiter (falling back to
// ',' when it is empty). Used for arrays and for composite literals.
PackedStringArray split_cell(const String &p_cell, const String &p_array_delimiter) {
	if (!p_array_delimiter.is_empty() && p_array_delimiter != ",") {
		return p_cell.split(p_array_delimiter);
	}
	return p_cell.split(",");
}

// Strips an optional "TypeName( ... )" wrapper.
String strip_wrapper(const String &p_cell) {
	String s = p_cell.strip_edges();
	int64_t open = s.find("(");
	if (open != -1) {
		int64_t close = s.rfind(")");
		if (close > open) {
			return s.substr(open + 1, close - open - 1).strip_edges();
		}
	}
	return s;
}

// Splits on `p_delim`, ignoring delimiters inside "( ... )" groups so nested
// literals like "Vector3(1,2,3), Vector3(4,5,6)" split at the top level.
PackedStringArray split_respecting_parens(const String &p_s, const String &p_delim) {
	PackedStringArray out;
	const char32_t delim_c = (p_delim.length() == 1) ? p_delim[0] : U',';
	String cur;
	int64_t depth = 0;
	for (int64_t i = 0; i < p_s.length(); i++) {
		const char32_t c = p_s[i];
		if (c == U'(') {
			depth++;
		} else if (c == U')') {
			if (depth > 0) {
				depth--;
			}
		}
		if (c == delim_c && depth == 0) {
			out.push_back(cur);
			cur = String();
			continue;
		}
		cur += c;
	}
	out.push_back(cur);
	return out;
}

// Parses a list of numeric components. Returns false if any component is not a
// valid number. Accepts "1,2,3", "Vector2(1,2)" and nested forms like
// "AABB(Vector3(1,2,3), Vector3(4,5,6))".
bool parse_numbers(const String &p_cell, const String &p_array_delimiter, std::vector<double> &r_vals) {
	String s = strip_wrapper(p_cell);
	PackedStringArray parts = split_respecting_parens(s, p_array_delimiter);
	for (int64_t i = 0; i < parts.size(); i++) {
		String part = parts[i].strip_edges();
		if (part.is_empty()) {
			continue;
		}
		if (part.find("(") != -1 || part.find(")") != -1) {
			// A component is itself a nested literal; flatten it.
			if (!parse_numbers(part, ",", r_vals)) {
				return false;
			}
			continue;
		}
		if (!part.is_valid_float()) {
			return false;
		}
		r_vals.push_back(part.to_float());
	}
	return !r_vals.empty();
}

bool parse_bool_str(const String &p_cell, bool &r_ok) {
	String t = p_cell.strip_edges().to_lower();
	if (t == "true" || t == "1" || t == "yes" || t == "on") {
		r_ok = true;
		return true;
	}
	if (t == "false" || t == "0" || t == "no" || t == "off") {
		r_ok = true;
		return false;
	}
	r_ok = false;
	return false;
}

bool parse_color(const String &p_cell, godot::Color &r_color) {
	String s = p_cell.strip_edges();
	if (s.begins_with("#")) {
		// Validate the format first ("#RRGGBB" or "#RRGGBBAA") so a legit
		// alpha==0 color isn't mistaken for a parse failure.
		const String hex = s.substr(1);
		if ((hex.length() == 6 || hex.length() == 8) && hex.is_valid_hex_number(false)) {
			r_color = godot::Color::from_string(s, godot::Color());
			return true;
		}
		return false;
	}
	std::vector<double> vals;
	if (parse_numbers(s, ",", vals) && (vals.size() == 3 || vals.size() == 4)) {
		r_color = godot::Color((float)vals[0], (float)vals[1], (float)vals[2], vals.size() == 4 ? (float)vals[3] : 1.0f);
		return true;
	}
	// Named colors (e.g. "red", "white").
	godot::Color named = godot::Color::from_string(s, godot::Color(0, 0, 0, 1));
	// A very rough "named color found" heuristic: alpha == 1 and not pure-black
	// unless the name is "black".
	if (named != godot::Color(0, 0, 0, 1) || s.to_lower() == "black") {
		r_color = named;
		return true;
	}
	return false;
}

// Resolves an enum value from a cell against PROPERTY_HINT_ENUM hint_string.
bool parse_enum(const String &p_cell, const String &p_hint_string, int64_t &r_value) {
	PackedStringArray entries = p_hint_string.split(",", false);
	godot::HashMap<String, int64_t> name_to_value;
	for (int64_t i = 0; i < entries.size(); i++) {
		String entry = entries[i].strip_edges();
		if (entry.is_empty()) {
			continue;
		}
		// Either "Name" (value = index) or "Value:Name".
		int64_t colon = entry.rfind(":");
		String name = entry;
		int64_t value = i;
		if (colon != -1) {
			String left = entry.substr(0, colon).strip_edges();
			String right = entry.substr(colon + 1).strip_edges();
			if (!left.is_empty() && left.is_valid_int()) {
				value = left.to_int();
				name = right;
			} else if (!right.is_empty() && right.is_valid_int()) {
				value = right.to_int();
				name = left;
			}
		}
		name_to_value[name.to_lower()] = value;
	}

	String key = p_cell.strip_edges();
	if (key.is_valid_int()) {
		r_value = key.to_int();
		return true;
	}
	if (name_to_value.has(key.to_lower())) {
		r_value = name_to_value[key.to_lower()];
		return true;
	}
	return false;
}

Variant parse_packed_ints(const String &p_cell, const String &p_delim, bool p_int64) {
	std::vector<double> vals;
	if (!parse_numbers(p_cell, p_delim, vals)) {
		return Variant();
	}
	if (p_int64) {
		godot::PackedInt64Array out;
		for (double v : vals) {
			out.push_back((int64_t)v);
		}
		return Variant(out);
	}
	godot::PackedInt32Array out;
	for (double v : vals) {
		out.push_back((int32_t)v);
	}
	return Variant(out);
}

Variant parse_packed_floats(const String &p_cell, const String &p_delim, bool p_64) {
	std::vector<double> vals;
	if (!parse_numbers(p_cell, p_delim, vals)) {
		return Variant();
	}
	if (p_64) {
		godot::PackedFloat64Array out;
		for (double v : vals) {
			out.push_back(v);
		}
		return Variant(out);
	}
	godot::PackedFloat32Array out;
	for (double v : vals) {
		out.push_back((float)v);
	}
	return Variant(out);
}

Variant parse_to_type_impl(const String &p_cell, const PropertyInfo &p_prop, const ConvertContext &p_ctx, String &r_err);

// Godot's Variant::get_type_by_name is case-sensitive and its canonical names
// are mixed-case ("String", "Vector2", "Color", ...). Our inference emits
// lowercase canonical names ("string", "int", "bool", ...) and header-schema
// types may arrive in either case, so resolve case-insensitively. Returns
// Variant::NIL when the name is unknown.
Variant::Type resolve_type_name(const String &p_name) {
	const Variant::Type direct = Variant::get_type_by_name(p_name);
	if (direct != Variant::VARIANT_MAX) {
		return direct;
	}
	const String lower = p_name.to_lower();
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (Variant::get_type_name((Variant::Type)i).to_lower() == lower) {
			return (Variant::Type)i;
		}
	}
	return Variant::NIL;
}

// Resolves the element type of a typed array property. Godot reports typed
// arrays either as PROPERTY_HINT_ARRAY_TYPE (hint_string = type name) or
// PROPERTY_HINT_TYPE_STRING (hint_string = "N:..." where N is Variant::Type).
Variant::Type array_element_type(const PropertyInfo &p_prop) {
	if (p_prop.hint == godot::PROPERTY_HINT_ARRAY_TYPE) {
		return resolve_type_name(p_prop.hint_string);
	}
	if (p_prop.hint == godot::PROPERTY_HINT_TYPE_STRING) {
		int64_t colon = p_prop.hint_string.find(":");
		if (colon > 0) {
			String num = p_prop.hint_string.substr(0, colon);
			if (num.is_valid_int()) {
				return (Variant::Type)num.to_int();
			}
		}
	}
	return Variant::NIL;
}

// Converts a Variant (already parsed, e.g. from a JSON array) into the target
// element type using the same cell-conversion semantics as the ";"-separated
// path. Variants whose type already matches are pushed unchanged; everything
// else is stringified and re-parsed so int/float/bool conversions follow the
// normal cell rules (e.g. JSON "1.0" -> int element "1").
Variant convert_array_element(const Variant &p_value, const PropertyInfo &p_elem_prop,
		const ConvertContext &p_ctx, String &r_err) {
	if (p_value.get_type() == p_elem_prop.type) {
		return p_value;
	}
	String cell;
	switch (p_value.get_type()) {
	case Variant::FLOAT: {
		const double f = static_cast<double>(p_value);
		if (f == static_cast<double>(static_cast<int64_t>(f)) &&
				f >= -9.2e18 && f <= 9.2e18) {
			cell = String::num_int64(static_cast<int64_t>(f));
		} else {
			cell = String::num(f);
		}
		break;
	}
	case Variant::BOOL:
		cell = bool(p_value) ? "true" : "false";
		break;
	case Variant::NIL:
		return Variant();
	default:
		cell = String(p_value);
		break;
	}
	return parse_to_type_impl(cell, p_elem_prop, p_ctx, r_err);
}

Variant parse_array_elements(const String &p_cell, const PropertyInfo &p_prop, const ConvertContext &p_ctx, String &r_err) {
	const Variant::Type elem_type = array_element_type(p_prop);
	PropertyInfo elem_prop = p_prop;
	if (elem_type != Variant::NIL) {
		elem_prop.type = elem_type;
		elem_prop.hint = godot::PROPERTY_HINT_NONE;
		elem_prop.hint_string = String();
	} else {
		// No element type info: fall back to string elements.
		elem_prop.type = Variant::STRING;
	}

	// JSON array literal? e.g. "[1,2,3]".
	String s = p_cell.strip_edges();
	if (s.begins_with("[")) {
		Variant json = godot::JSON::parse_string(s);
		if (json.get_type() == Variant::ARRAY) {
			Array arr = json;
			// JSON gives an untyped Array. When the property declares an element
			// type, rebuild it as a typed empty array and push each converted
			// element — calling set_typed() on a non-empty array raises "Type can
			// only be set when array is empty".
			if (elem_type == Variant::NIL) {
				return Variant(arr);
			}
			Array out;
			out.set_typed((uint32_t)elem_type, StringName(), Variant());
			for (int64_t i = 0; i < arr.size(); i++) {
				const Variant &item = arr[i];
				if (item.get_type() == Variant::NIL) {
					out.push_back(Variant());
					continue;
				}
				String elem_err;
				Variant v = convert_array_element(item, elem_prop, p_ctx, elem_err);
				if (v.get_type() == Variant::NIL && !elem_err.is_empty()) {
					r_err = elem_err;
					return Variant();
				}
				out.push_back(v);
			}
			return Variant(out);
		}
		r_err = "invalid JSON array: " + s;
		return Variant();
	}

	PackedStringArray parts = split_cell(p_cell, p_ctx.array_delimiter);

	Array out;
	if (elem_type != Variant::NIL) {
		out.set_typed((uint32_t)elem_type, StringName(), Variant());
	}
	for (int64_t i = 0; i < parts.size(); i++) {
		String part = parts[i];
		if (is_no_value(part, p_ctx.null_token)) {
			out.push_back(Variant());
			continue;
		}
		String elem_err;
		Variant v = parse_to_type_impl(part.strip_edges(), elem_prop, p_ctx, elem_err);
		if (v.get_type() == Variant::NIL && !elem_err.is_empty()) {
			r_err = elem_err;
			return Variant();
		}
		out.push_back(v);
	}
	return Variant(out);
}

Variant parse_to_type_impl(const String &p_cell, const PropertyInfo &p_prop, const ConvertContext &p_ctx, String &r_err) {
	// Enum properties are stored as int with an ENUM hint.
	if (p_prop.hint == godot::PROPERTY_HINT_ENUM) {
		int64_t value = 0;
		if (parse_enum(p_cell, p_prop.hint_string, value)) {
			return Variant(value);
		}
		r_err = "cannot parse enum value '" + p_cell + "'";
		return Variant();
	}

	switch (p_prop.type) {
	case Variant::NIL:
	case Variant::STRING:
		return Variant(p_cell);
	case Variant::STRING_NAME:
		return Variant(StringName(p_cell));
	case Variant::NODE_PATH:
		return Variant(godot::NodePath(p_cell));
	case Variant::BOOL: {
		bool ok = false;
		bool value = parse_bool_str(p_cell, ok);
		if (ok) {
			return Variant(value);
		}
		r_err = "cannot parse bool '" + p_cell + "'";
		return Variant();
	}
	case Variant::INT: {
		String s = p_cell.strip_edges();
		if (s.is_valid_int()) {
			return Variant(s.to_int());
		}
		r_err = "cannot parse int '" + p_cell + "'";
		return Variant();
	}
	case Variant::FLOAT: {
		String s = p_cell.strip_edges();
		if (s.is_valid_float()) {
			return Variant(s.to_float());
		}
		r_err = "cannot parse float '" + p_cell + "'";
		return Variant();
	}
	case Variant::VECTOR2:
	case Variant::VECTOR2I: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 2) {
			if (p_prop.type == Variant::VECTOR2) {
				return Variant(godot::Vector2((float)vals[0], (float)vals[1]));
			}
			return Variant(godot::Vector2i((int32_t)vals[0], (int32_t)vals[1]));
		}
		r_err = "expected 2 numbers for Vector2, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::VECTOR3:
	case Variant::VECTOR3I: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 3) {
			if (p_prop.type == Variant::VECTOR3) {
				return Variant(godot::Vector3((float)vals[0], (float)vals[1], (float)vals[2]));
			}
			return Variant(godot::Vector3i((int32_t)vals[0], (int32_t)vals[1], (int32_t)vals[2]));
		}
		r_err = "expected 3 numbers for Vector3, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::VECTOR4:
	case Variant::VECTOR4I: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 4) {
			if (p_prop.type == Variant::VECTOR4) {
				return Variant(godot::Vector4((float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3]));
			}
			return Variant(godot::Vector4i((int32_t)vals[0], (int32_t)vals[1], (int32_t)vals[2], (int32_t)vals[3]));
		}
		r_err = "expected 4 numbers for Vector4, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::RECT2:
	case Variant::RECT2I: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 4) {
			if (p_prop.type == Variant::RECT2) {
				return Variant(godot::Rect2((float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3]));
			}
			return Variant(godot::Rect2i((int32_t)vals[0], (int32_t)vals[1], (int32_t)vals[2], (int32_t)vals[3]));
		}
		r_err = "expected 4 numbers for Rect2, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::COLOR: {
		godot::Color color;
		if (parse_color(p_cell, color)) {
			return Variant(color);
		}
		r_err = "cannot parse Color '" + p_cell + "'";
		return Variant();
	}
	case Variant::PLANE: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 4) {
			return Variant(godot::Plane((float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3]));
		}
		r_err = "expected 4 numbers for Plane, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::QUATERNION: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 4) {
			return Variant(godot::Quaternion((float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3]));
		}
		r_err = "expected 4 numbers for Quaternion, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::AABB: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 6) {
			return Variant(godot::AABB(godot::Vector3((float)vals[0], (float)vals[1], (float)vals[2]),
					godot::Vector3((float)vals[3], (float)vals[4], (float)vals[5])));
		}
		r_err = "expected 6 numbers for AABB, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::BASIS: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 9) {
			godot::Basis b;
			b.set_column(0, godot::Vector3((float)vals[0], (float)vals[1], (float)vals[2]));
			b.set_column(1, godot::Vector3((float)vals[3], (float)vals[4], (float)vals[5]));
			b.set_column(2, godot::Vector3((float)vals[6], (float)vals[7], (float)vals[8]));
			return Variant(b);
		}
		r_err = "expected 9 numbers for Basis, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::TRANSFORM2D: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 6) {
			godot::Transform2D t;
			t.columns[0] = godot::Vector2((float)vals[0], (float)vals[1]);
			t.columns[1] = godot::Vector2((float)vals[2], (float)vals[3]);
			t.columns[2] = godot::Vector2((float)vals[4], (float)vals[5]);
			return Variant(t);
		}
		r_err = "expected 6 numbers for Transform2D, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::TRANSFORM3D: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 12) {
			godot::Transform3D t;
			t.basis.set_column(0, godot::Vector3((float)vals[0], (float)vals[1], (float)vals[2]));
			t.basis.set_column(1, godot::Vector3((float)vals[3], (float)vals[4], (float)vals[5]));
			t.basis.set_column(2, godot::Vector3((float)vals[6], (float)vals[7], (float)vals[8]));
			t.origin = godot::Vector3((float)vals[9], (float)vals[10], (float)vals[11]);
			return Variant(t);
		}
		r_err = "expected 12 numbers for Transform3D, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::PROJECTION: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, ",", vals) && vals.size() == 16) {
			godot::Projection p;
			for (int r = 0; r < 4; r++) {
				p.columns[r] = godot::Vector4((float)vals[r * 4], (float)vals[r * 4 + 1], (float)vals[r * 4 + 2], (float)vals[r * 4 + 3]);
			}
			return Variant(p);
		}
		r_err = "expected 16 numbers for Projection, got '" + p_cell + "'";
		return Variant();
	}
	case Variant::DICTIONARY: {
		Variant json = godot::JSON::parse_string(p_cell.strip_edges());
		if (json.get_type() == Variant::DICTIONARY) {
			return json;
		}
		r_err = "invalid JSON dictionary: " + p_cell;
		return Variant();
	}
	case Variant::ARRAY:
		return parse_array_elements(p_cell, p_prop, p_ctx, r_err);
	case Variant::PACKED_BYTE_ARRAY: {
		std::vector<double> vals;
		if (parse_numbers(p_cell, p_ctx.array_delimiter, vals)) {
			godot::PackedByteArray out;
			for (double v : vals) {
				out.push_back((uint8_t)v);
			}
			return Variant(out);
		}
		r_err = "cannot parse PackedByteArray '" + p_cell + "'";
		return Variant();
	}
	case Variant::PACKED_INT32_ARRAY:
		return parse_packed_ints(p_cell, p_ctx.array_delimiter, false);
	case Variant::PACKED_INT64_ARRAY:
		return parse_packed_ints(p_cell, p_ctx.array_delimiter, true);
	case Variant::PACKED_FLOAT32_ARRAY:
		return parse_packed_floats(p_cell, p_ctx.array_delimiter, false);
	case Variant::PACKED_FLOAT64_ARRAY:
		return parse_packed_floats(p_cell, p_ctx.array_delimiter, true);
	case Variant::PACKED_STRING_ARRAY: {
		PackedStringArray parts = split_cell(p_cell, p_ctx.array_delimiter);
		godot::PackedStringArray out;
		for (int64_t i = 0; i < parts.size(); i++) {
			out.push_back(parts[i].strip_edges());
		}
		return Variant(out);
	}
	case Variant::PACKED_VECTOR2_ARRAY:
	case Variant::PACKED_VECTOR3_ARRAY:
	case Variant::PACKED_VECTOR4_ARRAY:
	case Variant::PACKED_COLOR_ARRAY: {
		PackedStringArray parts = split_cell(p_cell, p_ctx.array_delimiter);
		int expected = (p_prop.type == Variant::PACKED_VECTOR2_ARRAY) ? 2 : 3;
		if (p_prop.type == Variant::PACKED_VECTOR4_ARRAY || p_prop.type == Variant::PACKED_COLOR_ARRAY) {
			expected = 4;
		}
		if (p_prop.type == Variant::PACKED_VECTOR2_ARRAY) {
			godot::PackedVector2Array out;
			for (int64_t i = 0; i < parts.size(); i++) {
				std::vector<double> vals;
				if (!parse_numbers(parts[i], ",", vals) || vals.size() != expected) {
					r_err = "expected " + String::num_int64(expected) + " numbers per element, got '" + parts[i] + "'";
					return Variant();
				}
				out.push_back(godot::Vector2((float)vals[0], (float)vals[1]));
			}
			return Variant(out);
		}
		if (p_prop.type == Variant::PACKED_VECTOR3_ARRAY) {
			godot::PackedVector3Array out;
			for (int64_t i = 0; i < parts.size(); i++) {
				std::vector<double> vals;
				if (!parse_numbers(parts[i], ",", vals) || vals.size() != expected) {
					r_err = "expected " + String::num_int64(expected) + " numbers per element, got '" + parts[i] + "'";
					return Variant();
				}
				out.push_back(godot::Vector3((float)vals[0], (float)vals[1], (float)vals[2]));
			}
			return Variant(out);
		}
		if (p_prop.type == Variant::PACKED_VECTOR4_ARRAY) {
			godot::PackedVector4Array out;
			for (int64_t i = 0; i < parts.size(); i++) {
				std::vector<double> vals;
				if (!parse_numbers(parts[i], ",", vals) || vals.size() != expected) {
					r_err = "expected " + String::num_int64(expected) + " numbers per element, got '" + parts[i] + "'";
					return Variant();
				}
				out.push_back(godot::Vector4((float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3]));
			}
			return Variant(out);
		}
		godot::PackedColorArray out;
		for (int64_t i = 0; i < parts.size(); i++) {
			godot::Color c;
			if (!parse_color(parts[i], c)) {
				r_err = "cannot parse Color element '" + parts[i] + "'";
				return Variant();
			}
			out.push_back(c);
		}
		return Variant(out);
	}
	case Variant::OBJECT: {
		if (is_no_value(p_cell, p_ctx.null_token)) {
			return Variant();
		}
		if (p_ctx.object_resolver) {
			Variant resolved = p_ctx.object_resolver(p_cell, p_prop.class_name);
			if (resolved.get_type() == Variant::OBJECT) {
				return resolved;
			}
			// A non-empty OBJECT cell that failed to resolve is a data error —
			// surface it instead of silently keeping the default (null).
			r_err = "unresolved foreign key '" + p_cell + "' -> " + String(p_prop.class_name);
			return Variant();
		}
		r_err = "no resolver configured for OBJECT cell '" + p_cell + "'";
		return Variant();
	}
	default:
		r_err = "unsupported target type " + Variant::get_type_name(p_prop.type);
		return Variant();
	}
}

} // namespace

Variant parse_to_type(const String &p_cell, const PropertyInfo &p_prop, const ConvertContext &p_ctx, String &r_err) {
	r_err = String();
	if (is_no_value(p_cell, p_ctx.null_token)) {
		return Variant(); // NIL, no error: caller keeps the default.
	}
	return parse_to_type_impl(p_cell, p_prop, p_ctx, r_err);
}

Variant parse_to_type(const String &p_cell, Variant::Type p_type, const ConvertContext &p_ctx, String &r_err) {
	PropertyInfo prop(p_type, StringName());
	return parse_to_type(p_cell, prop, p_ctx, r_err);
}

bool property_for_type_name(const String &p_type_name, PropertyInfo &r_out) {
	if (p_type_name.ends_with("[]")) {
		r_out.type = Variant::ARRAY;
		r_out.hint = PROPERTY_HINT_ARRAY_TYPE;
		r_out.hint_string = p_type_name.substr(0, p_type_name.length() - 2);
		return true;
	}
	r_out.type = resolve_type_name(p_type_name);
	return r_out.type != Variant::NIL;
}

Variant coerce_typed_array(const Variant &p_value, const PropertyInfo &p_prop,
		const ConvertContext &p_ctx, String &r_err) {
	if (p_value.get_type() != Variant::ARRAY) {
		return p_value;
	}
	Array arr = p_value;
	const Variant::Type elem_type = array_element_type(p_prop);
	if (elem_type == Variant::NIL || arr.get_typed_builtin() == (uint32_t)elem_type) {
		return p_value; // no declared element type, or already typed to match.
	}
	// Object arrays are left to Godot's own assignment type-checks.
	if (elem_type == Variant::OBJECT || elem_type == Variant::NIL) {
		return p_value;
	}

	PropertyInfo elem_prop = p_prop;
	elem_prop.type = elem_type;
	elem_prop.hint = godot::PROPERTY_HINT_NONE;
	elem_prop.hint_string = String();

	Array out;
	out.set_typed((uint32_t)elem_type, StringName(), Variant());
	for (int64_t i = 0; i < arr.size(); i++) {
		const Variant &item = arr[i];
		if (item.get_type() == Variant::NIL) {
			out.push_back(Variant());
			continue;
		}
		String elem_err;
		Variant v = convert_array_element(item, elem_prop, p_ctx, elem_err);
		if (v.get_type() == Variant::NIL && !elem_err.is_empty()) {
			r_err = elem_err;
			return Variant();
		}
		out.push_back(v);
	}
	return Variant(out);
}

} // namespace vortariscsv
