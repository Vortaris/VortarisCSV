#include "csv_writer.h"

#include <vector>

namespace vortariscsv {

using godot::PackedStringArray;
using godot::String;

bool CsvWriteOptions::resolve(String &r_error) const {
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

	return true;
}

String csv_write_rows(const std::vector<PackedStringArray> &p_rows, const CsvWriteOptions &p_opts) {
	std::vector<char32_t> out;
	out.reserve(1024);

	const char32_t delim = p_opts.delimiter_c;
	const char32_t quote = p_opts.quote_c;

	// Line ending as a code-point sequence.
	std::vector<char32_t> le;
	le.reserve((size_t)p_opts.line_ending.length());
	for (int64_t i = 0; i < p_opts.line_ending.length(); i++) {
		le.push_back(p_opts.line_ending[i]);
	}

	const bool has_delim = p_opts.delimiter.length() == 1;
	const bool has_quote = p_opts.quote.length() == 1;

	for (const PackedStringArray &row : p_rows) {
		for (int64_t c = 0; c < row.size(); c++) {
			if (c > 0) {
				out.push_back(delim);
			}
			const String field = row[c];

			bool needs_quote = p_opts.always_quote;
			if (!needs_quote) {
				if (has_delim && field.find(p_opts.delimiter) != -1) {
					needs_quote = true;
				}
				if (!needs_quote && (field.find("\r") != -1 || field.find("\n") != -1)) {
					needs_quote = true;
				}
				if (!needs_quote && has_quote && field.find(p_opts.quote) != -1) {
					needs_quote = true;
				}
			}

			if (p_opts.sanitize_formulas && !field.is_empty()) {
				const char32_t first = field[0];
				if (first == U'=' || first == U'+' || first == U'@') {
					out.push_back(U'\'');
				}
			}

			if (needs_quote) {
				out.push_back(quote);
				for (int64_t i = 0; i < field.length(); i++) {
					const char32_t ch = field[i];
					if (ch == quote) {
						out.push_back(quote);
					}
					out.push_back(ch);
				}
				out.push_back(quote);
			} else {
				for (int64_t i = 0; i < field.length(); i++) {
					out.push_back(field[i]);
				}
			}
		}
		for (char32_t ch : le) {
			out.push_back(ch);
		}
	}

	out.push_back(0);
	return String(out.data());
}

} // namespace vortariscsv
