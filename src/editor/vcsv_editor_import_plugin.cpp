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
	delimiter["hint_string"] = "Comma,Tab,Semicolon,Space";
	options.push_back(delimiter);

	Dictionary quote;
	quote["name"] = "quote";
	quote["default_value"] = "\"";
	options.push_back(quote);

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
	parse_opts.delimiter = delimiter_string_for(static_cast<int64_t>(p_options["delimiter"]));
	parse_opts.quote = static_cast<String>(p_options["quote"]);
	parse_opts.has_header = static_cast<bool>(p_options["has_header"]);
	parse_opts.trim_whitespace = static_cast<bool>(p_options["trim_whitespace"]);
	parse_opts.skip_blank_lines = static_cast<bool>(p_options["skip_blank_lines"]);
	parse_opts.comment_prefix = static_cast<String>(p_options["comment_prefix"]);
	parse_opts.strict = false;

	// --- Read the file (encoding-aware) and parse. ---
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	String encoding = static_cast<String>(p_options["encoding"]).to_lower();
	String text;
	if (encoding == "gbk" || encoding == "gb2312") {
		text = vortariscsv::gbk_bytes_to_string(bytes);
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
		UtilityFunctions::push_error("VortarisCSV import failed for " + p_source_file + ": " + parse_error.message);
		return err;
	}

	// --- Build the VCSVDataTable resource. ---
	Ref<VCSVDataTable> table;
	table.instantiate();
	PackedStringArray headers;
	Array data_rows;
	if (parse_opts.has_header && !rows.empty()) {
		headers = rows[0];
		for (size_t i = 1; i < rows.size(); i++) {
			data_rows.push_back(rows[i]);
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
	if (static_cast<bool>(p_options["detect_types"])) {
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
