#include "vcsv_editor_import_plugin.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../core/csv_parser.h"
#include "../core/gbk.h"
#include "../core/type_inference.h"
#include "../gdscript/vcsv_data_table.h"

namespace godot {

enum DelimiterOption {
	DELIM_COMMA = 0,
	DELIM_TAB = 1,
	DELIM_SEMICOLON = 2,
	DELIM_SPACE = 3,
	DELIM_CUSTOM = 4,
};

namespace {
String delimiter_string_for(int64_t p_enum_value) {
	switch (p_enum_value) {
	case DELIM_TAB:
		return "\t";
	case DELIM_SEMICOLON:
		return ";";
	case DELIM_SPACE:
		return " ";
	default:
		return ",";
	}
}

// Parses the "column_types" import option: "hp:int;attack:float" (pairs split on
// ';' / newlines, each pair split at the first ':'). Invalid pairs are skipped.
Dictionary parse_column_types_text(const String &p_text) {
	Dictionary out;
	if (p_text.is_empty()) {
		return out;
	}
	String normalized = p_text.replace("\n", ";").replace("\r", "").replace("\t", "");
	PackedStringArray pairs = normalized.split(";", false);
	for (int64_t i = 0; i < pairs.size(); i++) {
		const String pair = pairs[i].strip_edges();
		if (pair.is_empty()) {
			continue;
		}
		const int64_t colon = pair.find(":");
		if (colon <= 0) {
			continue;
		}
		const String name = pair.substr(0, colon).strip_edges();
		const String type = pair.substr(colon + 1).strip_edges();
		if (name.is_empty() || type.is_empty()) {
			continue;
		}
		out[name] = type;
	}
	return out;
}
} // namespace

VCSVEditorImportPlugin::VCSVEditorImportPlugin() {}

String VCSVEditorImportPlugin::_get_importer_name() const {
	return "vortariscsv";
}

String VCSVEditorImportPlugin::_get_visible_name() const {
	return "Vortaris CSV Data";
}

int32_t VCSVEditorImportPlugin::_get_preset_count() const {
	return 2;
}

String VCSVEditorImportPlugin::_get_preset_name(int32_t p_preset_index) const {
	return p_preset_index == 1 ? "TSV" : "CSV";
}

PackedStringArray VCSVEditorImportPlugin::_get_recognized_extensions() const {
	PackedStringArray exts;
	exts.push_back("csv");
	exts.push_back("tsv");
	return exts;
}

TypedArray<Dictionary> VCSVEditorImportPlugin::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options;
	const bool is_tsv = p_preset_index == 1;

	Dictionary delimiter;
	delimiter["name"] = "delimiter";
	delimiter["default_value"] = is_tsv ? DELIM_TAB : DELIM_COMMA;
	delimiter["property_hint"] = PROPERTY_HINT_ENUM;
	delimiter["hint_string"] = "Comma,Tab,Semicolon,Space,Custom";
	options.push_back(delimiter);

	Dictionary delimiter_custom;
	delimiter_custom["name"] = "delimiter_custom";
	delimiter_custom["default_value"] = ",";
	delimiter_custom["property_hint"] = PROPERTY_HINT_PLACEHOLDER_TEXT;
	delimiter_custom["hint_string"] = "single character (used when delimiter = Custom)";
	options.push_back(delimiter_custom);

	Dictionary quote;
	quote["name"] = "quote";
	quote["default_value"] = "\"";
	options.push_back(quote);

	Dictionary auto_detect_delimiter;
	auto_detect_delimiter["name"] = "auto_detect_delimiter";
	auto_detect_delimiter["default_value"] = false;
	options.push_back(auto_detect_delimiter);

	Dictionary delimiter_candidates;
	delimiter_candidates["name"] = "delimiter_candidates";
	delimiter_candidates["default_value"] = ",;\t|";
	delimiter_candidates["property_hint"] = PROPERTY_HINT_PLACEHOLDER_TEXT;
	delimiter_candidates["hint_string"] = "candidates for auto-detect";
	options.push_back(delimiter_candidates);

	Dictionary header_rows;
	header_rows["name"] = "header_rows";
	header_rows["default_value"] = 1;
	options.push_back(header_rows);

	Dictionary header_join;
	header_join["name"] = "header_join";
	header_join["default_value"] = ".";
	options.push_back(header_join);

	Dictionary encoding;
	encoding["name"] = "encoding";
	encoding["default_value"] = "utf8";
	encoding["property_hint"] = PROPERTY_HINT_ENUM;
	encoding["hint_string"] = "utf8,gbk,gb2312";
	options.push_back(encoding);

	Dictionary has_header;
	has_header["name"] = "has_header";
	has_header["default_value"] = true;
	options.push_back(has_header);

	Dictionary key_column;
	key_column["name"] = "key_column";
	key_column["default_value"] = "";
	key_column["property_hint"] = PROPERTY_HINT_PLACEHOLDER_TEXT;
	key_column["hint_string"] = "empty = first column";
	options.push_back(key_column);

	Dictionary row_type;
	row_type["name"] = "row_type";
	row_type["default_value"] = "";
	row_type["property_hint"] = PROPERTY_HINT_FILE;
	row_type["hint_string"] = "*.gd,*.cs";
	options.push_back(row_type);

	Dictionary detect_types;
	detect_types["name"] = "detect_types";
	detect_types["default_value"] = true;
	options.push_back(detect_types);

	Dictionary column_types;
	column_types["name"] = "column_types";
	column_types["default_value"] = "";
	column_types["property_hint"] = PROPERTY_HINT_PLACEHOLDER_TEXT;
	column_types["hint_string"] = "explicit types, e.g. hp:int;attack:float (wins over detection)";
	options.push_back(column_types);

	Dictionary trim_whitespace;
	trim_whitespace["name"] = "trim_whitespace";
	trim_whitespace["default_value"] = true;
	options.push_back(trim_whitespace);

	Dictionary skip_blank_lines;
	skip_blank_lines["name"] = "skip_blank_lines";
	skip_blank_lines["default_value"] = true;
	options.push_back(skip_blank_lines);

	Dictionary comment_prefix;
	comment_prefix["name"] = "comment_prefix";
	comment_prefix["default_value"] = "";
	options.push_back(comment_prefix);

	Dictionary array_delimiter;
	array_delimiter["name"] = "array_delimiter";
	array_delimiter["default_value"] = ";";
	options.push_back(array_delimiter);

	Dictionary null_token;
	null_token["name"] = "null_token";
	null_token["default_value"] = "";
	options.push_back(null_token);

	Dictionary case_insensitive;
	case_insensitive["name"] = "case_insensitive_columns";
	case_insensitive["default_value"] = false;
	options.push_back(case_insensitive);

	return options;
}

String VCSVEditorImportPlugin::_get_save_extension() const {
	return "tres";
}

String VCSVEditorImportPlugin::_get_resource_type() const {
	return "VCSVDataTable";
}

float VCSVEditorImportPlugin::_get_priority() const {
	// Dynamic override: when vortariscsv/import/override_translation_importer is
	// enabled (default), claim .csv/.tsv ahead of Godot's built-in translation
	// importer. When disabled, drop below it so the built-in wins by default;
	// per-asset switching stays available via the Import dock's "Import As".
	const bool override_builtin = ProjectSettings::get_singleton()->get_setting(
			"vortariscsv/import/override_translation_importer", true);
	return override_builtin ? 2.0f : 0.5f;
}

int32_t VCSVEditorImportPlugin::_get_import_order() const {
	return 0;
}

bool VCSVEditorImportPlugin::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const {
	if (p_option_name == StringName("delimiter_custom")) {
		// Only show the custom-character field when the delimiter enum is Custom.
		if (p_options.has("delimiter")) {
			return static_cast<int64_t>(p_options["delimiter"]) == DELIM_CUSTOM;
		}
		return false;
	}
	return true;
}

bool VCSVEditorImportPlugin::_can_import_threaded() const {
	return false;
}

Error VCSVEditorImportPlugin::_import(const String &p_source_file, const String &p_save_path,
		const Dictionary &p_options, const TypedArray<String> &p_platform_variants,
		const TypedArray<String> &p_gen_files) const {
	// --- Build parse options from the import panel. ---
	vortariscsv::CsvParseOptions parse_opts;
	if (static_cast<int64_t>(p_options["delimiter"]) == DELIM_CUSTOM) {
		parse_opts.delimiter = static_cast<String>(p_options["delimiter_custom"]);
	} else {
		parse_opts.delimiter = delimiter_string_for(static_cast<int64_t>(p_options["delimiter"]));
	}
	parse_opts.quote = static_cast<String>(p_options["quote"]);
	parse_opts.has_header = static_cast<bool>(p_options["has_header"]);
	parse_opts.trim_whitespace = static_cast<bool>(p_options["trim_whitespace"]);
	parse_opts.skip_blank_lines = static_cast<bool>(p_options["skip_blank_lines"]);
	parse_opts.comment_prefix = static_cast<String>(p_options["comment_prefix"]);
	parse_opts.auto_detect_delimiter = static_cast<bool>(p_options["auto_detect_delimiter"]);
	parse_opts.delimiter_candidates = static_cast<String>(p_options["delimiter_candidates"]);
	parse_opts.header_rows = static_cast<int64_t>(p_options["header_rows"]);
	parse_opts.header_join = static_cast<String>(p_options["header_join"]);
	parse_opts.strict = false;

	// --- Read the file (encoding-aware) and parse. ---
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	String encoding = static_cast<String>(p_options["encoding"]).to_lower();
	String text;
	if (encoding == "gbk" || encoding == "gb2312") {
		int64_t bom = 0;
		if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
			bom = 3;
		}
		text = vortariscsv::gbk_bytes_to_string(bytes.ptr() + bom, bytes.size() - bom);
	} else {
		int64_t offset = 0;
		if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
			offset = 3;
		}
		text = String::utf8((const char *)bytes.ptr() + offset, bytes.size() - offset);
	}

	std::vector<PackedStringArray> rows;
	std::vector<String> warnings;
	vortariscsv::CsvParseError parse_error;
	Error err = vortariscsv::csv_parse(text, parse_opts, rows, warnings, parse_error);
	if (err != OK) {
		String location;
		if (parse_error.line > 0) {
			location = " (line " + String::num_int64(parse_error.line);
			if (parse_error.column > 0) {
				location += ", col " + String::num_int64(parse_error.column);
			}
			location += ")";
		}
		UtilityFunctions::push_error("VortarisCSV import failed for " + p_source_file + ": " +
				parse_error.message + location);
		return err;
	}

	// --- Build the VCSVDataTable resource. ---
	Ref<VCSVDataTable> table;
	table.instantiate();
	PackedStringArray headers;
	Array data_rows;
	if (parse_opts.has_header && !rows.empty()) {
		if (parse_opts.header_rows > 1) {
			headers = vortariscsv::join_header_rows(rows, parse_opts.header_rows, parse_opts.header_join);
			for (size_t i = (size_t)parse_opts.header_rows; i < rows.size(); i++) {
				data_rows.push_back(rows[i]);
			}
		} else {
			headers = rows[0];
			for (size_t i = 1; i < rows.size(); i++) {
				data_rows.push_back(rows[i]);
			}
		}
	} else {
		for (const PackedStringArray &row : rows) {
			data_rows.push_back(row);
		}
	}
	table->set_headers(headers);
	table->set_rows(data_rows);

	String key_column = static_cast<String>(p_options["key_column"]);
	if (key_column.is_empty() && !headers.is_empty()) {
		key_column = headers[0];
	}
	table->set_key_column(key_column);
	table->set_row_type(static_cast<String>(p_options["row_type"]));
	table->set_array_delimiter(static_cast<String>(p_options["array_delimiter"]));
	table->set_null_token(static_cast<String>(p_options["null_token"]));
	table->set_case_insensitive_columns(static_cast<bool>(p_options["case_insensitive_columns"]));

	// --- Infer column types and bake them into the .tres. ---
	// Explicit "column_types" text wins over auto-detection. Otherwise infer only
	// when there is no row_type: with a row_type, the row object's declared
	// property types take precedence, and an auto-inferred override could fight
	// them (e.g. a float inference overriding an int property).
	const String row_type = static_cast<String>(p_options["row_type"]);
	const String column_types_text = static_cast<String>(p_options["column_types"]).strip_edges();
	if (!column_types_text.is_empty()) {
		table->set_column_types(parse_column_types_text(column_types_text));
	} else if (row_type.is_empty() && static_cast<bool>(p_options["detect_types"])) {
		std::vector<PackedStringArray> data_rows_cpp;
		for (int64_t i = 0; i < data_rows.size(); i++) {
			const Variant &v = data_rows[i];
			if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
				data_rows_cpp.push_back(PackedStringArray(v));
			}
		}
		vortariscsv::InferOptions io;
		io.array_delimiter = table->get_array_delimiter();
		io.detect_booleans = true;
		Dictionary types = vortariscsv::infer_column_types(headers, data_rows_cpp, io);
		table->set_column_types(types);
	}

	// --- Save as .tres. ---
	String save_path = p_save_path;
	save_path += ".";
	save_path += _get_save_extension();
	Error save_err = ResourceSaver::get_singleton()->save(table, save_path,
			ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS);
	if (save_err != OK) {
		UtilityFunctions::push_error("VortarisCSV failed to save " + save_path);
		return save_err;
	}
	return OK;
}

void VCSVEditorImportPlugin::_bind_methods() {
	// No exposed methods — the editor drives this through the virtual overrides.
}

} // namespace godot
