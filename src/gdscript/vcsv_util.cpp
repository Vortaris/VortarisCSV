#include "vcsv_util.h"

#include <vector>

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "../core/type_converter.h"
#include "../core/type_inference.h"
#include "vcsv_parser.h"

namespace godot {

VCSVUtil::VCSVUtil() {}

Ref<VCSVParseResult> VCSVUtil::parse(const String &p_text, const Ref<VCSVParseOptions> &p_options) {
	return VCSVParser::parse_string(p_text, p_options);
}

Ref<VCSVParseResult> VCSVUtil::parse_file(const String &p_path, const Ref<VCSVParseOptions> &p_options) {
	return VCSVParser::parse_file(p_path, p_options);
}

Dictionary VCSVUtil::detect_types(const Ref<VCSVTable> &p_table, const String &p_array_delimiter, bool p_detect_booleans) {
	Dictionary out;
	if (p_table.is_null()) {
		return out;
	}
	vortariscsv::InferOptions io;
	io.array_delimiter = p_array_delimiter;
	io.detect_booleans = p_detect_booleans;

	std::vector<PackedStringArray> rows;
	for (int64_t i = 0; i < p_table->get_rows().size(); i++) {
		const Variant &v = p_table->get_rows()[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			rows.push_back(PackedStringArray(v));
		}
	}
	return vortariscsv::infer_column_types(p_table->get_headers(), rows, io);
}

Array VCSVUtil::table_to_dict_array(const Ref<VCSVTable> &p_table, const String &p_array_delimiter,
		const Dictionary &p_explicit_types) {
	Array out;
	if (p_table.is_null() || p_table->get_headers().is_empty()) {
		return out;
	}
	const PackedStringArray headers = p_table->get_headers();

	std::vector<PackedStringArray> rows;
	for (int64_t i = 0; i < p_table->get_rows().size(); i++) {
		const Variant &v = p_table->get_rows()[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			rows.push_back(PackedStringArray(v));
		}
	}
	vortariscsv::InferOptions io;
	io.array_delimiter = p_array_delimiter;
	io.detect_booleans = true; // typed convenience loader recognizes true/false
	Dictionary types = vortariscsv::infer_column_types(headers, rows, io);
	// Header-schema (header_type_separator) types declared at parse time win.
	for (int64_t i = 0; i < p_explicit_types.size(); i++) {
		types[p_explicit_types.keys()[i]] = p_explicit_types.values()[i];
	}

	vortariscsv::ConvertContext ctx;
	ctx.array_delimiter = p_array_delimiter;
	for (int64_t r = 0; r < p_table->get_row_count(); r++) {
		PackedStringArray row = p_table->get_row(r);
		Dictionary dict;
		for (int64_t c = 0; c < row.size() && c < headers.size(); c++) {
			const String &cell = row[c];
			const String type_name = String(types[headers[c]]);
			Variant value;
			if (type_name == "json") {
				Variant parsed = JSON::parse_string(cell.strip_edges());
				value = parsed.get_type() == Variant::NIL ? Variant(cell) : parsed;
			} else {
				PropertyInfo prop;
				if (vortariscsv::property_for_type_name(type_name, prop)) {
					String err;
					value = vortariscsv::parse_to_type(cell, prop, ctx, err);
					if (value.get_type() == Variant::NIL && !err.is_empty()) {
						value = Variant(cell); // fall back to the raw string
					}
				} else {
					value = Variant(cell);
				}
			}
			dict[headers[c]] = value;
		}
		out.push_back(dict);
	}
	return out;
}

Array VCSVUtil::load_csv_dict_array(const String &p_csv_path, const Ref<VCSVParseOptions> &p_options) {
	Ref<VCSVParseResult> result = VCSVParser::parse_file(p_csv_path, p_options);
	if (result.is_null() || !result->get_success()) {
		return Array();
	}
	return table_to_dict_array(result->get_table(), ";", result->get_column_types());
}

Dictionary VCSVUtil::load_csv_dict(const String &p_csv_path, const Ref<VCSVParseOptions> &p_options) {
	Array rows = load_csv_dict_array(p_csv_path, p_options);
	if (rows.is_empty()) {
		return Dictionary();
	}
	const Variant &first = rows[0];
	if (first.get_type() == Variant::DICTIONARY) {
		return Dictionary(first);
	}
	return Dictionary();
}

String VCSVUtil::type_name(const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		return "null";
	}
	return Variant::get_type_name(p_value.get_type());
}

void VCSVUtil::_bind_methods() {
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("parse", "text", "options"), &VCSVUtil::parse);
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("parse_file", "path", "options"), &VCSVUtil::parse_file);
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("detect_types", "table", "array_delimiter", "detect_booleans"),
			&VCSVUtil::detect_types, DEFVAL(";"), DEFVAL(false));
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("load_csv_dict_array", "csv_path", "options"),
			&VCSVUtil::load_csv_dict_array, DEFVAL(Variant()));
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("load_csv_dict", "csv_path", "options"),
			&VCSVUtil::load_csv_dict, DEFVAL(Variant()));
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("table_to_dict_array", "table", "array_delimiter", "explicit_types"),
			&VCSVUtil::table_to_dict_array, DEFVAL(";"), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("type_name", "value"), &VCSVUtil::type_name);
}

} // namespace godot
