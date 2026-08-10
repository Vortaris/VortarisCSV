#pragma once

#include <vector>

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Serialization options mirroring the GDScript-facing VCSVWriter.
struct CsvWriteOptions {
	// Field separator (single code point; empty defaults to ',').
	godot::String delimiter = ",";
	// Quote character (single code point; empty defaults to '"').
	godot::String quote = "\"";
	// Line ending appended after every record. Default CRLF per RFC 4180.
	godot::String line_ending = "\r\n";
	// Quote every field, even when not required.
	bool always_quote = false;
	// Prefix fields starting with '=', '+', '@' with a single quote to prevent
	// spreadsheet formula injection. ('-' is deliberately excluded so that
	// negative numbers stay numeric.)
	bool sanitize_formulas = false;

	// Resolved single-code-point forms (filled by `resolve()`).
	mutable char32_t delimiter_c = U',';
	mutable char32_t quote_c = U'"';

	// Returns false (and fills p_error) when delimiter/quote are multi-char.
	bool resolve(godot::String &r_error) const;
};

// Serializes `p_rows` (each a PackedStringArray) into a DSV/CSV string,
// applying quoting/escaping per RFC 4180.
godot::String csv_write_rows(const std::vector<godot::PackedStringArray> &p_rows,
		const CsvWriteOptions &p_opts);

} // namespace vortariscsv
