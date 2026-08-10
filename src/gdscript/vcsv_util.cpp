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

Array VCSVUtil::load_csv_dict_array(const String &p_csv_path, const Ref<VCSVParseOptions> &p_options) {
	Array out;
	Ref<VCSVParseResult> result = VCSVParser::parse_file(p_csv_path, p_options);
	if (result.is_null() || !result->get_success()) {
		return out;
	}
	Ref<VCSVTable> table = result->get_table();
	if (table.is_null() || table->get_headers().is_empty()) {
		return out;
	}
	PackedStringArray headers = table->get_headers();

	std::vector<PackedStringArray> rows;
	for (int64_t i = 0; i < table->get_rows().size(); i++) {
		const Variant &v = table->get_rows()[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			rows.push_back(PackedStringArray(v));
		}
	}
	vortariscsv::InferOptions io;
	io.detect_booleans = true; // typed convenience loader recognizes true/false
	Dictionary types = vortariscsv::infer_column_types(headers, rows, io);

	vortariscsv::ConvertContext ctx;
	for (int64_t r = 0; r < table->get_row_count(); r++) {
		PackedStringArray row = table->get_row(r);
		Dictionary dict;
		for (int64_t c = 0; c < row.size() && c < headers.size(); c++) {
			const String &cell = row[c];
			String type_name = String(types[headers[c]]);
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
	ClassDB::bind_static_method("VCSVUtil", D_METHOD("type_name", "value"), &VCSVUtil::type_name);
}

} // namespace godot
