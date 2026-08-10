#include "csv_parser.h"

#include <cstring>

#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

using godot::ERR_INVALID_PARAMETER;
using godot::ERR_PARSE_ERROR;
using godot::Error;
using godot::OK;
using godot::PackedStringArray;
using godot::String;

bool CsvParseOptions::resolve(String &r_error) const {
	if (delimiter.is_empty()) {
		delimiter_c = U',';
	} else if (delimiter.length() == 1) {
		delimiter_c = delimiter[0];
	} else {
		r_error = "delimiter must be exactly one character";
		return false;
	}

	if (quote.is_empty()) {
		quote_c = U'"';
	} else if (quote.length() == 1) {
		quote_c = quote[0];
	} else {
		r_error = "quote must be exactly one character";
		return false;
	}

	if (!comment_prefix.is_empty()) {
		if (comment_prefix.length() == 1) {
			comment_prefix_c = comment_prefix[0];
		} else {
			r_error = "comment_prefix must be exactly one character";
			return false;
		}
	} else {
		comment_prefix_c = 0; // disabled
	}

	return true;
}

namespace {

// Internal single-pass RFC 4180 state machine.
class Parser {
public:
	Parser(const String &p_text, const CsvParseOptions &p_opts) :
			text_(p_text), opts_(p_opts) {}

	Error run(std::vector<PackedStringArray> &r_out_rows, std::vector<String> &r_warnings,
			CsvParseError &r_error);

private:
	enum class State { OUTSIDE, IN_QUOTES, AFTER_QUOTES };

	void flush_field();
	bool flush_record(std::vector<PackedStringArray> &r_out_rows, std::vector<String> &r_warnings);
	void fail(const String &p_message);

	const String &text_;
	const CsvParseOptions &opts_;

	std::vector<char32_t> field_;
	PackedStringArray row_;

	State state_ = State::OUTSIDE;
	bool field_quoted_ = false;
	bool at_field_start_ = true; // next char begins a new field (leading-trim context)
	bool at_record_start_ = true; // next char begins a fresh record (comment context)
	bool skip_to_eol_ = false;
	int64_t expected_width_ = -1;
	int64_t line_ = 1;
	int64_t col_ = 0; // 0-based position of the char about to be consumed
	int64_t record_start_line_ = 1;
	int64_t quote_open_line_ = 0;
	int64_t quote_open_col_ = 0;

	bool aborted_ = false;
	CsvParseError *error_out_ = nullptr;
};

inline bool is_ws(char32_t c) {
	return c == U' ' || c == U'\t' || c == 0x0B || c == 0x0C || c == 0x00A0;
}

void Parser::fail(const String &p_message) {
	if (!aborted_) {
		aborted_ = true;
		if (error_out_) {
			error_out_->has_error = true;
			error_out_->line = line_;
			error_out_->column = col_ + 1;
			error_out_->message = p_message;
		}
	}
}

void Parser::flush_field() {
	// Trailing whitespace is stripped only for unquoted fields.
	if (opts_.trim_whitespace && !field_quoted_) {
		while (!field_.empty() && is_ws(field_.back())) {
			field_.pop_back();
		}
	}

	// Build the field from its code-point buffer. The char32_t* String
	// constructor (string_new_with_utf32_chars) is the reliable bulk path —
	// resize()+ptrw()+memcpy relies on internal buffer layout and is not safe.
	String s;
	if (!field_.empty()) {
		field_.push_back(0); // null terminator for the char32 constructor
		s = String(field_.data());
		field_.pop_back();
	}
	row_.push_back(s);
	field_.clear();
	field_quoted_ = false;
	at_field_start_ = true;
	at_record_start_ = false;
}

bool Parser::flush_record(std::vector<PackedStringArray> &r_out_rows, std::vector<String> &r_warnings) {
	if (aborted_) {
		return false;
	}

	const bool blank = opts_.skip_blank_lines && row_.size() == 1 && row_[0].is_empty();
	if (!blank) {
		if (expected_width_ < 0) {
			expected_width_ = (int64_t)row_.size();
		} else if ((int64_t)row_.size() != expected_width_) {
			if (opts_.strict) {
				error_out_->has_error = true;
				error_out_->line = record_start_line_;
				error_out_->column = 1;
				error_out_->message = "Row has " + String::num_int64((int64_t)row_.size()) +
						" fields but the first row has " + String::num_int64(expected_width_);
				aborted_ = true;
				return false;
			}
			if ((int64_t)row_.size() < expected_width_) {
				while ((int64_t)row_.size() < expected_width_) {
					row_.push_back(String());
				}
				r_warnings.push_back(String::num_int64(record_start_line_) + ":1: Row is shorter than the first row; padded with empty fields");
			} else {
				PackedStringArray truncated;
				for (int64_t k = 0; k < expected_width_; k++) {
					truncated.push_back(row_[k]);
				}
				row_ = truncated;
				r_warnings.push_back(String::num_int64(record_start_line_) + ":1: Row is longer than the first row; trailing fields truncated");
			}
		}
		r_out_rows.push_back(row_);
	}

	row_ = PackedStringArray();
	at_record_start_ = true;
	at_field_start_ = true;
	field_quoted_ = false;
	field_.clear();
	return true;
}

Error Parser::run(std::vector<PackedStringArray> &r_out_rows, std::vector<String> &r_warnings,
		CsvParseError &r_error) {
	error_out_ = &r_error;

	const char32_t *p = text_.ptr();
	const int64_t len = text_.length();
	const char32_t delim = opts_.delimiter_c;
	const char32_t quote = opts_.quote_c;
	const bool has_comment = opts_.comment_prefix_c != 0;
	const char32_t comment = opts_.comment_prefix_c;
	const bool trim = opts_.trim_whitespace;

	// Strip a leading U+FEFF (UTF-8 BOM decoded) once.
	int64_t start = 0;
	if (opts_.strip_bom && len > 0 && p[0] == 0xFEFF) {
		start = 1;
	}

	int64_t i = start;
	while (i < len && !aborted_) {
		const char32_t c = p[i];
		col_ = i;

		if (skip_to_eol_) {
			if (c == U'\n' || c == U'\r') {
				skip_to_eol_ = false;
				i++;
				if (c == U'\r' && i < len && p[i] == U'\n') {
					i++;
				}
				line_++;
				col_ = 0;
				at_record_start_ = true;
				at_field_start_ = true;
			} else {
				i++;
			}
			continue;
		}

		switch (state_) {
		case State::OUTSIDE:
			if (at_record_start_ && has_comment && c == comment) {
				skip_to_eol_ = true;
				i++;
				continue;
			}
			if (c == U'\n' || c == U'\r') {
				record_start_line_ = line_;
				flush_field();
				flush_record(r_out_rows, r_warnings);
				i++;
				if (c == U'\r' && i < len && p[i] == U'\n') {
					i++;
				}
				line_++;
				col_ = 0;
				continue;
			}
			if (at_field_start_ && trim && is_ws(c)) {
				// Leading whitespace of an unquoted field is skipped.
				i++;
				continue;
			}
			if (c == quote) {
				if (at_field_start_ || field_.empty()) {
					state_ = State::IN_QUOTES;
					field_quoted_ = true;
					at_field_start_ = false;
					quote_open_line_ = line_;
					quote_open_col_ = col_ + 1;
					i++;
					continue;
				}
				// A quote in the middle of an unquoted field.
				if (opts_.strict) {
					fail("Unexpected quote character inside an unquoted field");
					return ERR_PARSE_ERROR;
				}
				field_.push_back(c);
				i++;
				continue;
			}
			if (c == delim) {
				flush_field();
				i++;
				continue;
			}
			field_.push_back(c);
			at_field_start_ = false;
			i++;
			continue;

		case State::IN_QUOTES:
			if (c == quote) {
				if (i + 1 < len && p[i + 1] == quote) {
					field_.push_back(quote);
					i += 2;
					continue;
				}
				state_ = State::AFTER_QUOTES;
				i++;
				continue;
			}
			field_.push_back(c);
			i++;
			continue;

		case State::AFTER_QUOTES:
			if (c == delim) {
				flush_field();
				state_ = State::OUTSIDE;
				i++;
				continue;
			}
			if (c == U'\n' || c == U'\r') {
				record_start_line_ = line_;
				flush_field();
				flush_record(r_out_rows, r_warnings);
				state_ = State::OUTSIDE;
				i++;
				if (c == U'\r' && i < len && p[i] == U'\n') {
					i++;
				}
				line_++;
				col_ = 0;
				continue;
			}
			if (trim && is_ws(c)) {
				// Whitespace after a closing quote is tolerated (and skipped)
				// when trimming is enabled.
				i++;
				continue;
			}
			if (opts_.strict) {
				fail("Unexpected character after a quoted field");
				return ERR_PARSE_ERROR;
			}
			// Lenient: keep the character as part of the field.
			field_.push_back(c);
			state_ = State::OUTSIDE;
			i++;
			continue;
		}
	}

	if (state_ == State::IN_QUOTES && !aborted_) {
		aborted_ = true;
		r_error.has_error = true;
		r_error.line = quote_open_line_;
		r_error.column = quote_open_col_;
		r_error.message = "Unterminated quoted field (missing closing quote)";
		return ERR_PARSE_ERROR;
	}

	if (!aborted_ && (!field_.empty() || row_.size() > 0)) {
		flush_field();
		flush_record(r_out_rows, r_warnings);
	}

	if (aborted_) {
		return ERR_PARSE_ERROR;
	}
	return OK;
}

} // namespace

Error csv_parse(const String &p_text, const CsvParseOptions &p_opts,
		std::vector<PackedStringArray> &r_out_rows,
		std::vector<String> &r_warnings,
		CsvParseError &r_error) {
	r_out_rows.clear();
	r_warnings.clear();
	r_error = CsvParseError();

	String resolve_error;
	if (!p_opts.resolve(resolve_error)) {
		r_error.has_error = true;
		r_error.line = 0;
		r_error.column = 0;
		r_error.message = resolve_error;
		return ERR_INVALID_PARAMETER;
	}

	Parser parser(p_text, p_opts);
	return parser.run(r_out_rows, r_warnings, r_error);
}

} // namespace vortariscsv
