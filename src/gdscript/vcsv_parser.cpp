#include "vcsv_parser.h"

#include <vector>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../core/csv_parser.h"
#include "../core/gbk.h"
#include "../core/type_inference.h"
#include "../core/vcsv_log.h"
#include "vcsv_table.h"

namespace godot {

VCSVParser::VCSVParser() {}

namespace {
void copy_warnings(const std::vector<String> &p_src, PackedStringArray &p_dst) {
	for (const String &w : p_src) {
		p_dst.push_back(w);
	}
}

Ref<VCSVParseResult> make_result(bool p_success, int p_error, const String &p_message,
		int64_t p_line, int64_t p_col, const std::vector<String> &p_warnings) {
	Ref<VCSVParseResult> result;
	result.instantiate();
	result->set_success(p_success);
	result->set_error(p_error);
	result->set_message(p_message);
	result->set_error_line(p_line);
	result->set_error_column(p_col);
	PackedStringArray warnings;
	copy_warnings(p_warnings, warnings);
	result->set_warnings(warnings);
	return result;
}
} // namespace

Ref<VCSVParseResult> VCSVParser::parse_string(const String &p_text, const Ref<VCSVParseOptions> &p_options) {
	vortariscsv::CsvParseOptions opts;
	if (p_options.is_valid()) {
		opts = p_options->to_core();
	}

	std::vector<PackedStringArray> rows;
	std::vector<String> warnings;
	vortariscsv::CsvParseError error;

	Error err = vortariscsv::csv_parse(p_text, opts, rows, warnings, error);
	if (err != OK) {
		return make_result(false, err, error.message, error.line, error.column, warnings);
	}

	Ref<VCSVTable> table;
	table.instantiate();
	PackedStringArray headers;
	Array data_rows;
	if (opts.has_header && !rows.empty()) {
		if (opts.header_rows > 1) {
			// Multi-level header: join the first `header_rows` rows.
			headers = vortariscsv::join_header_rows(rows, opts.header_rows, opts.header_join);
			for (size_t i = (size_t)opts.header_rows; i < rows.size(); i++) {
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

	// Header schema: when header_type_separator is set, "hp:int" headers are
	// stripped to "hp" and the declared type is exposed as column_types.
	Dictionary explicit_types;
	if (!opts.header_type_separator.is_empty() && opts.has_header && !headers.is_empty()) {
		PackedStringArray stripped;
		for (int64_t i = 0; i < headers.size(); i++) {
			String name, type;
			if (vortariscsv::split_header_type(headers[i], opts.header_type_separator, name, type)) {
				stripped.push_back(name);
				explicit_types[name] = vortariscsv::is_valid_canonical_type_name(type) ? type : "string";
			} else {
				stripped.push_back(headers[i]);
			}
		}
		headers = stripped;
	}
	table->set_data(headers, data_rows);

	Ref<VCSVParseResult> result = make_result(true, OK, String(), 0, 0, warnings);
	result->set_table(table);
	result->set_column_types(explicit_types);

	VCSV_LOG_VERBOSE("parsed string -> " + String::num_int64(data_rows.size()) + " rows x " +
			String::num_int64(headers.size()) + " cols (delimiter='" + opts.delimiter +
			"' quote='" + opts.quote + "' has_header=" + (opts.has_header ? "true" : "false") +
			" warnings=" + String::num_int64(warnings.size()) + ")");
	return result;
}

Ref<VCSVParseResult> VCSVParser::parse_file(const String &p_path, const Ref<VCSVParseOptions> &p_options) {
	vortariscsv::CsvParseOptions opts;
	if (p_options.is_valid()) {
		opts = p_options->to_core();
	}

	if (!FileAccess::file_exists(p_path)) {
		return make_result(false, ERR_FILE_NOT_FOUND, "Cannot open file (not found): " + p_path, 0, 0, {});
	}

	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_path);
	if (bytes.is_empty()) {
		// A genuinely empty file is legal CSV (zero rows).
	}

	// Encoding-aware decode. GBK/GB2312 have no UTF-8 BOM, so BOM stripping only
	// applies to the UTF-8 path.
	String encoding = "utf8";
	if (p_options.is_valid()) {
		encoding = p_options->get_encoding().to_lower();
	}
	String text;
	if (encoding == "gbk" || encoding == "gb2312") {
		// Some tools prepend a UTF-8 BOM even to GBK files; strip it first.
		int64_t bom = 0;
		if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
			bom = 3;
		}
		text = vortariscsv::gbk_bytes_to_string(bytes.ptr() + bom, bytes.size() - bom);
	} else {
		int64_t offset = 0;
		if (opts.strip_bom && bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
			offset = 3;
		}
		text = String::utf8((const char *)bytes.ptr() + offset, bytes.size() - offset);
	}
	Ref<VCSVParseResult> result = parse_string(text, p_options);
	if (result.is_valid() && result->get_success() && result->get_table().is_valid()) {
		VCSV_LOG_VERBOSE("parsed " + p_path + " -> " +
				String::num_int64(result->get_table()->get_row_count()) + " rows x " +
				String::num_int64(result->get_table()->get_col_count()) + " cols");
	}
	return result;
}

void VCSVParser::_bind_methods() {
	ClassDB::bind_static_method("VCSVParser", D_METHOD("parse_string", "text", "options"),
			&VCSVParser::parse_string);
	ClassDB::bind_static_method("VCSVParser", D_METHOD("parse_file", "path", "options"),
			&VCSVParser::parse_file);
}

} // namespace godot
