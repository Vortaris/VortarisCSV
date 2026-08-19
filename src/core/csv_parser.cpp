#include "csv_parser.h"

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

// Quote-aware scan of the first `p_max_records` records of `p_text`, counting
// the number of records whose field count equals the candidate's most common
// width. Returns -1 when the candidate never produces >1 field (i.e. it is not
// actually present in the file).
int64_t delimiter_consistency(const String &p_text, char32_t p_delim, char32_t p_quote, int64_t p_max_records) {
	const char32_t *p = p_text.ptr();
	const int64_t len = p_text.length();
	int64_t i = 0;
	if (len > 0 && p[0] == 0xFEFF) {
		i = 1; // skip UTF-8 BOM
	}
	bool in_quotes = false;
	int64_t fields = 1;
	int64_t records = 0;
	int64_t width_counts[32] = { 0 }; // field counts 1..32 (32+ collapsed)
	int64_t max_width = 0;

	auto record_end = [&]() {
		if (fields <= 32) {
			width_counts[fields - 1]++;
			if (fields > max_width) {
				max_width = fields;
			}
		}
		records++;
		fields = 1;
	};

	while (i < len && records < p_max_records) {
		const char32_t c = p[i];
		if (in_quotes) {
			if (c == p_quote) {
				if (i + 1 < len && p[i + 1] == p_quote) {
					i += 2;
					continue;
				}
				in_quotes = false;
			}
			i++;
			continue;
		}
		if (c == p_quote) {
			in_quotes = true;
			i++;
			continue;
		}
		if (c == p_delim) {
			fields++;
			i++;
			continue;
		}
		if (c == U'\n' || c == U'\r') {
			record_end();
			if (c == U'\r' && i + 1 < len && p[i + 1] == U'\n') {
				i++;
			}
			i++;
			continue;
		}
		i++;
	}
	if (records < p_max_records && (fields > 1 || i > 0)) {
		record_end();
	}

	// A candidate that only ever yields single-field records is not a delimiter.
	if (max_width <= 1) {
		return -1;
	}
	int64_t best_index = 0;
	for (int64_t w = 1; w < max_width; w++) {
		if (width_counts[w] > width_counts[best_index]) {
			best_index = w;
		}
	}
	return width_counts[best_index];
}

// Auto-detects the delimiter from the first ~8 records by quote-aware width
// consistency. Returns the winning candidate (or `p_fallback` when none wins).
String auto_detect_delimiter(const String &p_text, const String &p_candidates,
		char32_t p_quote, const String &p_fallback) {
	String best = p_fallback;
	int64_t best_score = -1;
	for (int64_t i = 0; i < p_candidates.length(); i++) {
		const char32_t cand = p_candidates[i];
		if (cand == p_quote) {
			continue;
		}
		const int64_t score = delimiter_consistency(p_text, cand, p_quote, 8);
		if (score > best_score) {
			best_score = score;
			best = String::chr(cand);
		}
	}
	return best;
}

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
	// Records a hard error (first one wins as THE reported error; all of them
	// land in the warnings as "line:col message"). Returns false — and sets
	// aborted_ — once the max_errors budget is exhausted.
	bool note_hard_error(const String &p_message, int64_t p_line, int64_t p_col);

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
	int64_t hard_errors_ = 0;
	CsvParseError *error_out_ = nullptr;
	std::vector<String> *warnings_out_ = nullptr;
};

inline bool is_ws(char32_t c) {
	return c == U' ' || c == U'\t' || c == 0x0B || c == 0x0C || c == 0x00A0;
}

bool Parser::note_hard_error(const String &p_message, int64_t p_line, int64_t p_col) {
	hard_errors_++;
	if (error_out_ && error_out_->message.is_empty()) {
		error_out_->has_error = true;
		error_out_->line = p_line;
		error_out_->column = p_col;
		error_out_->message = p_message;
	}
	if (warnings_out_) {
		warnings_out_->push_back(String::num_int64(p_line) + ":" + String::num_int64(p_col) +
				" error: " + p_message);
	}
	if (opts_.max_errors > 0 && hard_errors_ >= opts_.max_errors) {
		aborted_ = true;
		return false;
	}
	return true;
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
				// Count the mismatch as a hard error. Below the max_errors budget
				// parsing continues (the row is normalized below); at the budget
				// the parse aborts. Either way a strict parse with any width
				// mismatch ends as ERR_PARSE_ERROR (see run()'s epilogue).
				if (!note_hard_error("Row has " + String::num_int64((int64_t)row_.size()) +
								" fields but the first row has " + String::num_int64(expected_width_),
							record_start_line_, 1)) {
					return false;
				}
			}
			if ((int64_t)row_.size() < expected_width_) {
				while ((int64_t)row_.size() < expected_width_) {
					row_.push_back(String());
				}
				if (!opts_.strict) {
					r_warnings.push_back(String::num_int64(record_start_line_) + ":1: Row is shorter than the first row; padded with empty fields");
				}
			} else {
				PackedStringArray truncated;
				for (int64_t k = 0; k < expected_width_; k++) {
					truncated.push_back(row_[k]);
				}
				row_ = truncated;
				if (!opts_.strict) {
					r_warnings.push_back(String::num_int64(record_start_line_) + ":1: Row is longer than the first row; trailing fields truncated");
				}
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
	warnings_out_ = &r_warnings;

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
					if (!note_hard_error("Unexpected quote character inside an unquoted field", line_, col_ + 1)) {
						return ERR_PARSE_ERROR;
					}
					// Below the error budget, continue leniently: literal quote.
					field_.push_back(c);
					i++;
					continue;
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
				if (!note_hard_error("Unexpected character after a quoted field", line_, col_ + 1)) {
					return ERR_PARSE_ERROR;
				}
				// Below the error budget, continue leniently: keep the character.
			}
			// Lenient: keep the character as part of the field.
			field_.push_back(c);
			state_ = State::OUTSIDE;
			i++;
			continue;
		}
	}

	if (state_ == State::IN_QUOTES && !aborted_) {
		// An unterminated quote swallows the rest of the file — there is no
		// meaningful way to continue, so this is always immediately fatal (it
		// still counts against the max_errors tally for reporting).
		note_hard_error("Unterminated quoted field (missing closing quote)", quote_open_line_, quote_open_col_);
		aborted_ = true;
		return ERR_PARSE_ERROR;
	}

	if (!aborted_ && (!field_.empty() || row_.size() > 0)) {
		flush_field();
		flush_record(r_out_rows, r_warnings);
	}

	// Strict parses that stayed under the max_errors budget collected their hard
	// errors without aborting — the outcome is still failure (r_error carries
	// the first one, the warnings carry all of them).
	if (aborted_ || hard_errors_ > 0) {
		return ERR_PARSE_ERROR;
	}
	return OK;
}

} // namespace

PackedStringArray join_header_rows(const std::vector<PackedStringArray> &p_rows,
		int64_t p_header_rows, const String &p_join) {
	PackedStringArray out;
	if (p_rows.empty()) {
		return out;
	}
	const int64_t width = p_rows[0].size();
	if (p_header_rows <= 1) {
		return p_rows[0];
	}
	if ((int64_t)p_rows.size() < p_header_rows) {
		p_header_rows = (int64_t)p_rows.size();
	}
	for (int64_t c = 0; c < width; c++) {
		String parts;
		for (int64_t r = 0; r < p_header_rows; r++) {
			if (r > 0) {
				parts += p_join;
			}
			parts += (c < p_rows[(size_t)r].size()) ? p_rows[(size_t)r][c] : String();
		}
		out.push_back(parts);
	}
	return out;
}

Error csv_parse(const String &p_text, const CsvParseOptions &p_opts,
		std::vector<PackedStringArray> &r_out_rows,
		std::vector<String> &r_warnings,
		CsvParseError &r_error) {
	r_out_rows.clear();
	r_warnings.clear();
	r_error = CsvParseError();

	// Work on a mutable copy so delimiter auto-detection can adjust the option
	// before resolution.
	CsvParseOptions opts = p_opts;

	String resolve_error;
	if (!opts.resolve(resolve_error)) {
		r_error.has_error = true;
		r_error.line = 0;
		r_error.column = 0;
		r_error.message = resolve_error;
		return ERR_INVALID_PARAMETER;
	}

	// Auto-detect the delimiter from the first ~8 records when requested.
	if (opts.auto_detect_delimiter) {
		String detected = auto_detect_delimiter(p_text, opts.delimiter_candidates, opts.quote_c, opts.delimiter);
		if (detected != opts.delimiter) {
			opts.delimiter = detected;
			r_warnings.push_back("auto-detected delimiter '" + detected + "'");
			// Re-resolve so delimiter_c tracks the detected delimiter.
			if (!opts.resolve(resolve_error)) {
				r_error.has_error = true;
				r_error.line = 0;
				r_error.column = 0;
				r_error.message = resolve_error;
				return ERR_INVALID_PARAMETER;
			}
		}
	}

	Parser parser(p_text, opts);
	Error err = parser.run(r_out_rows, r_warnings, r_error);
	if (err != OK) {
		return err;
	}

	// Apply the row_offset / max_rows slice after parsing. The header row (when
	// present) is always kept; slicing is over data rows.
	if (opts.row_offset > 0 || opts.max_rows > 0) {
		const int64_t total = (int64_t)r_out_rows.size();
		const int64_t data_start = (opts.has_header && total > 0) ? 1 : 0;
		int64_t begin = data_start + opts.row_offset;
		if (begin > total) {
			begin = total;
		}
		int64_t end = (opts.max_rows > 0) ? begin + opts.max_rows : total;
		if (end > total) {
			end = total;
		}
		std::vector<PackedStringArray> sliced;
		sliced.reserve((size_t)(data_start + (end - begin)));
		if (data_start == 1) {
			sliced.push_back(r_out_rows[0]);
		}
		for (int64_t i = begin; i < end; i++) {
			sliced.push_back(r_out_rows[(size_t)i]);
		}
		r_out_rows.swap(sliced);
	}
	return OK;
}

} // namespace vortariscsv
