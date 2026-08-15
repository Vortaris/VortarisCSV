#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../core/csv_parser.h"

namespace godot {

// GDScript-facing parse options (a Resource so it can be embedded/saved).
// Maps 1:1 onto vortariscsv::CsvParseOptions.
class VCSVParseOptions : public Resource {
	GDCLASS(VCSVParseOptions, Resource)

public:
	VCSVParseOptions();

	String get_delimiter() const { return delimiter_; }
	void set_delimiter(const String &p_value) { delimiter_ = p_value; }

	String get_quote() const { return quote_; }
	void set_quote(const String &p_value) { quote_ = p_value; }

	String get_comment_prefix() const { return comment_prefix_; }
	void set_comment_prefix(const String &p_value) { comment_prefix_ = p_value; }

	bool get_trim_whitespace() const { return trim_whitespace_; }
	void set_trim_whitespace(bool p_value) { trim_whitespace_ = p_value; }

	bool get_strict() const { return strict_; }
	void set_strict(bool p_value) { strict_ = p_value; }

	bool get_skip_blank_lines() const { return skip_blank_lines_; }
	void set_skip_blank_lines(bool p_value) { skip_blank_lines_ = p_value; }

	bool get_strip_bom() const { return strip_bom_; }
	void set_strip_bom(bool p_value) { strip_bom_ = p_value; }

	bool get_has_header() const { return has_header_; }
	void set_has_header(bool p_value) { has_header_ = p_value; }

	bool get_case_insensitive_columns() const { return case_insensitive_columns_; }
	void set_case_insensitive_columns(bool p_value) { case_insensitive_columns_ = p_value; }

	// File encoding: "utf8" (default), "gbk" or "gb2312".
	String get_encoding() const { return encoding_; }
	void set_encoding(const String &p_value) { encoding_ = p_value; }

	// When non-empty, header cells of the form "name<sep>Type" (e.g. "hp:int")
	// are split: the annotation is stripped from the header name and the
	// declared type is exposed via VCSVParseResult.column_types / used by
	// load_csv_dict_array. Default empty (off) so plain ':' in headers survive.
	String get_header_type_separator() const { return header_type_separator_; }
	void set_header_type_separator(const String &p_value) { header_type_separator_ = p_value; }

	int64_t get_max_errors() const { return max_errors_; }
	void set_max_errors(int64_t p_value) { max_errors_ = p_value; }

	// Skip this many data rows after the header before keeping rows.
	int64_t get_row_offset() const { return row_offset_; }
	void set_row_offset(int64_t p_value) { row_offset_ = p_value; }

	// Keep at most this many data rows (0 = unlimited).
	int64_t get_max_rows() const { return max_rows_; }
	void set_max_rows(int64_t p_value) { max_rows_ = p_value; }

	// When true, the delimiter is auto-detected from the first ~8 records by
	// quote-aware width consistency across `delimiter_candidates`.
	bool get_auto_detect_delimiter() const { return auto_detect_delimiter_; }
	void set_auto_detect_delimiter(bool p_value) { auto_detect_delimiter_ = p_value; }

	// Candidate delimiters tried by auto-detect (single code points).
	String get_delimiter_candidates() const { return delimiter_candidates_; }
	void set_delimiter_candidates(const String &p_value) { delimiter_candidates_ = p_value; }

	// Number of leading rows that form the header. >1 joins them with
	// `header_join` into one header row (multi-level headers).
	int64_t get_header_rows() const { return header_rows_; }
	void set_header_rows(int64_t p_value) { header_rows_ = p_value; }

	// Separator used to join multi-level header rows.
	String get_header_join() const { return header_join_; }
	void set_header_join(const String &p_value) { header_join_ = p_value; }

	// Internal: project onto the core options struct.
	vortariscsv::CsvParseOptions to_core() const;

protected:
	static void _bind_methods();

private:
	String delimiter_ = ",";
	String quote_ = "\"";
	String comment_prefix_ = "";
	bool trim_whitespace_ = true;
	bool strict_ = false;
	bool skip_blank_lines_ = true;
	bool strip_bom_ = true;
	bool has_header_ = true;
	bool case_insensitive_columns_ = false;
	int64_t max_errors_ = 100;
	String encoding_ = "utf8";
	String header_type_separator_ = "";
	int64_t row_offset_ = 0;
	int64_t max_rows_ = 0;
	bool auto_detect_delimiter_ = false;
	String delimiter_candidates_ = ",;\t|";
	int64_t header_rows_ = 1;
	String header_join_ = ".";
};

} // namespace godot
