#include "vcsv_parse_options.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

VCSVParseOptions::VCSVParseOptions() {}

vortariscsv::CsvParseOptions VCSVParseOptions::to_core() const {
	vortariscsv::CsvParseOptions o;
	o.delimiter = delimiter_;
	o.quote = quote_;
	o.comment_prefix = comment_prefix_;
	o.trim_whitespace = trim_whitespace_;
	o.strict = strict_;
	o.skip_blank_lines = skip_blank_lines_;
	o.strip_bom = strip_bom_;
	o.has_header = has_header_;
	o.max_errors = max_errors_;
	o.header_type_separator = header_type_separator_;
	o.row_offset = row_offset_;
	o.max_rows = max_rows_;
	o.auto_detect_delimiter = auto_detect_delimiter_;
	o.delimiter_candidates = delimiter_candidates_;
	o.header_rows = header_rows_;
	o.header_join = header_join_;
	return o;
}

void VCSVParseOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_delimiter"), &VCSVParseOptions::get_delimiter);
	ClassDB::bind_method(D_METHOD("set_delimiter", "value"), &VCSVParseOptions::set_delimiter);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter"), "set_delimiter", "get_delimiter");

	ClassDB::bind_method(D_METHOD("get_quote"), &VCSVParseOptions::get_quote);
	ClassDB::bind_method(D_METHOD("set_quote", "value"), &VCSVParseOptions::set_quote);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "quote"), "set_quote", "get_quote");

	ClassDB::bind_method(D_METHOD("get_comment_prefix"), &VCSVParseOptions::get_comment_prefix);
	ClassDB::bind_method(D_METHOD("set_comment_prefix", "value"), &VCSVParseOptions::set_comment_prefix);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "comment_prefix"), "set_comment_prefix", "get_comment_prefix");

	ClassDB::bind_method(D_METHOD("get_trim_whitespace"), &VCSVParseOptions::get_trim_whitespace);
	ClassDB::bind_method(D_METHOD("set_trim_whitespace", "value"), &VCSVParseOptions::set_trim_whitespace);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trim_whitespace"), "set_trim_whitespace", "get_trim_whitespace");

	ClassDB::bind_method(D_METHOD("get_strict"), &VCSVParseOptions::get_strict);
	ClassDB::bind_method(D_METHOD("set_strict", "value"), &VCSVParseOptions::set_strict);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "strict"), "set_strict", "get_strict");

	ClassDB::bind_method(D_METHOD("get_skip_blank_lines"), &VCSVParseOptions::get_skip_blank_lines);
	ClassDB::bind_method(D_METHOD("set_skip_blank_lines", "value"), &VCSVParseOptions::set_skip_blank_lines);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "skip_blank_lines"), "set_skip_blank_lines", "get_skip_blank_lines");

	ClassDB::bind_method(D_METHOD("get_strip_bom"), &VCSVParseOptions::get_strip_bom);
	ClassDB::bind_method(D_METHOD("set_strip_bom", "value"), &VCSVParseOptions::set_strip_bom);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "strip_bom"), "set_strip_bom", "get_strip_bom");

	ClassDB::bind_method(D_METHOD("get_has_header"), &VCSVParseOptions::get_has_header);
	ClassDB::bind_method(D_METHOD("set_has_header", "value"), &VCSVParseOptions::set_has_header);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_header"), "set_has_header", "get_has_header");

	ClassDB::bind_method(D_METHOD("get_case_insensitive_columns"), &VCSVParseOptions::get_case_insensitive_columns);
	ClassDB::bind_method(D_METHOD("set_case_insensitive_columns", "value"), &VCSVParseOptions::set_case_insensitive_columns);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "case_insensitive_columns"), "set_case_insensitive_columns", "get_case_insensitive_columns");

	ClassDB::bind_method(D_METHOD("get_max_errors"), &VCSVParseOptions::get_max_errors);
	ClassDB::bind_method(D_METHOD("set_max_errors", "value"), &VCSVParseOptions::set_max_errors);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_errors"), "set_max_errors", "get_max_errors");

	ClassDB::bind_method(D_METHOD("get_encoding"), &VCSVParseOptions::get_encoding);
	ClassDB::bind_method(D_METHOD("set_encoding", "value"), &VCSVParseOptions::set_encoding);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "encoding"), "set_encoding", "get_encoding");

	ClassDB::bind_method(D_METHOD("get_header_type_separator"), &VCSVParseOptions::get_header_type_separator);
	ClassDB::bind_method(D_METHOD("set_header_type_separator", "value"), &VCSVParseOptions::set_header_type_separator);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "header_type_separator"), "set_header_type_separator", "get_header_type_separator");

	ClassDB::bind_method(D_METHOD("get_row_offset"), &VCSVParseOptions::get_row_offset);
	ClassDB::bind_method(D_METHOD("set_row_offset", "value"), &VCSVParseOptions::set_row_offset);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "row_offset"), "set_row_offset", "get_row_offset");

	ClassDB::bind_method(D_METHOD("get_max_rows"), &VCSVParseOptions::get_max_rows);
	ClassDB::bind_method(D_METHOD("set_max_rows", "value"), &VCSVParseOptions::set_max_rows);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_rows"), "set_max_rows", "get_max_rows");

	ClassDB::bind_method(D_METHOD("get_auto_detect_delimiter"), &VCSVParseOptions::get_auto_detect_delimiter);
	ClassDB::bind_method(D_METHOD("set_auto_detect_delimiter", "value"), &VCSVParseOptions::set_auto_detect_delimiter);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_detect_delimiter"), "set_auto_detect_delimiter", "get_auto_detect_delimiter");

	ClassDB::bind_method(D_METHOD("get_delimiter_candidates"), &VCSVParseOptions::get_delimiter_candidates);
	ClassDB::bind_method(D_METHOD("set_delimiter_candidates", "value"), &VCSVParseOptions::set_delimiter_candidates);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter_candidates"), "set_delimiter_candidates", "get_delimiter_candidates");

	ClassDB::bind_method(D_METHOD("get_header_rows"), &VCSVParseOptions::get_header_rows);
	ClassDB::bind_method(D_METHOD("set_header_rows", "value"), &VCSVParseOptions::set_header_rows);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "header_rows"), "set_header_rows", "get_header_rows");

	ClassDB::bind_method(D_METHOD("get_header_join"), &VCSVParseOptions::get_header_join);
	ClassDB::bind_method(D_METHOD("set_header_join", "value"), &VCSVParseOptions::set_header_join);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "header_join"), "set_header_join", "get_header_join");
}

} // namespace godot
