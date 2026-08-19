#pragma once

#include <vector>

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Internal parse options — a POD mirror of the GDScript-facing
// `VCSVParseOptions` (see src/gdscript/vcsv_parse_options.h). Kept in core so
// the parser itself never depends on Godot object classes.
struct CsvParseOptions {
	// Field separator. Must be exactly one UTF-32 code point (or empty for a
	// single-character default of ',').
	godot::String delimiter = ",";
	// Quote character. Must be exactly one code point (default: double quote).
	godot::String quote = "\"";
	// Non-empty: lines whose FIRST field starts with this prefix are skipped
	// (checked before the delimiter; a common choice is "#").
	godot::String comment_prefix = "";
	// Trim leading/trailing whitespace of UNQUOTED fields only. Quoted fields
	// keep their content verbatim.
	bool trim_whitespace = true;
	// true  -> a row whose field count differs from the first row is a hard
	//          error (ERR_PARSE_ERROR).
	// false -> short rows are padded with empty strings, long rows truncated,
	//          and a warning is recorded.
	bool strict = false;
	// Skip completely empty records (e.g. blank lines).
	bool skip_blank_lines = true;
	// Strip a leading UTF-8 byte order mark before parsing.
	bool strip_bom = true;
	// Whether the first data row is a header row. The parser itself only
	// separates rows; header handling is done by VCSVTable. Kept here for
	// import/serialization metadata.
	bool has_header = true;
	// When non-empty, header cells of the form "name<sep>Type" (e.g. "hp:int")
	// are split on the separator: the annotation is stripped from the header
	// name and the declared type is recorded by the caller. Default empty (off)
	// so plain headers containing the separator are never mangled.
	godot::String header_type_separator;
	// Hard-error budget. In strict mode, structural problems (row width
	// mismatch, stray quote, junk after a quoted field) are counted as hard
	// errors: parsing CONTINUES past each one (the row is normalized and the
	// error recorded) until `max_errors` is reached, then aborts with
	// ERR_PARSE_ERROR. The outcome of a strict parse with any hard error is
	// still failure — but the warnings carry up to `max_errors` problems
	// instead of only the first. 0 = unlimited (never abort early).
	// Unterminated quotes are always fatal immediately.
	int64_t max_errors = 100;
	// Slice: skip this many data rows after the header before keeping rows.
	int64_t row_offset = 0;
	// Slice: keep at most this many data rows (0 = unlimited).
	int64_t max_rows = 0;
	// When true, the delimiter is auto-detected from the first ~8 records by
	// quote-aware width consistency across `delimiter_candidates`.
	bool auto_detect_delimiter = false;
	// Candidate delimiters tried by auto-detect (single code points).
	godot::String delimiter_candidates = ",;\t|";
	// Number of leading rows that form the header. >1 joins them with
	// `header_join` into one header row (multi-level headers).
	int64_t header_rows = 1;
	// Separator used to join multi-level header rows.
	godot::String header_join = ".";

	// Resolved single-code-point forms (filled by `resolve()`, a const
	// operation, hence mutable).
	mutable char32_t delimiter_c = U',';
	mutable char32_t quote_c = U'"';
	mutable char32_t comment_prefix_c = U'#';

	// Validates and normalizes the string options into their char32 forms.
	// Returns false (and fills p_error) when an option is invalid.
	bool resolve(godot::String &r_error) const;
};

// Parse outcome: error (with location) or success plus non-fatal warnings.
struct CsvParseError {
	bool has_error = false;
	int64_t line = 0;    // 1-based; 0 when not applicable.
	int64_t column = 0;  // 1-based (in code points); 0 when not applicable.
	godot::String message;
};

// Parses `p_text` (a decoded String; BOM stripping is applied here when
// p_opts.strip_bom) as delimiter-separated values.
//
// On success returns OK and fills `r_out_rows` (one PackedStringArray per
// record, header row included when present). `r_warnings` collects non-fatal
// issues (each prefixed with "line:col " where relevant).
//
// On hard failure returns an Error (ERR_PARSE_ERROR for malformed input) and
// leaves the diagnostics in `r_error`. `r_out_rows` may hold a partial result.
godot::Error csv_parse(const godot::String &p_text, const CsvParseOptions &p_opts,
		std::vector<godot::PackedStringArray> &r_out_rows,
		std::vector<godot::String> &r_warnings,
		CsvParseError &r_error);

// Joins the first `p_header_rows` rows into a single header row: header[r][c]
// cells are concatenated with `p_join` between rows (e.g. "Level.Health").
godot::PackedStringArray join_header_rows(const std::vector<godot::PackedStringArray> &p_rows,
		int64_t p_header_rows, const godot::String &p_join);

} // namespace vortariscsv
