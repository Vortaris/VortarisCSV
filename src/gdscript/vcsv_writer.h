#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../core/csv_writer.h"
#include "vcsv_table.h"

namespace godot {

// Serializes VCSVTable / rows / dicts back into a DSV/CSV string or file.
class VCSVWriter : public RefCounted {
	GDCLASS(VCSVWriter, RefCounted)

public:
	VCSVWriter();

	String get_delimiter() const { return delimiter_; }
	void set_delimiter(const String &p_value) { delimiter_ = p_value; }

	String get_quote() const { return quote_; }
	void set_quote(const String &p_value) { quote_ = p_value; }

	String get_line_ending() const { return line_ending_; }
	void set_line_ending(const String &p_value) { line_ending_ = p_value; }

	bool get_always_quote() const { return always_quote_; }
	void set_always_quote(bool p_value) { always_quote_ = p_value; }

	bool get_sanitize_formulas() const { return sanitize_formulas_; }
	void set_sanitize_formulas(bool p_value) { sanitize_formulas_ = p_value; }

	// Output text encoding for file writes (0.4.0): "utf8" (default, no BOM),
	// "utf8_bom" (UTF-8 with BOM — Chinese Excel autodetects it) or
	// "gbk"/"gb2312" (symmetric with the parser's GBK decode). String-returning
	// methods are unaffected (they always produce Godot Strings).
	String get_encoding() const { return encoding_; }
	void set_encoding(const String &p_value) { encoding_ = p_value; }

	// Table (headers + data rows) to string / file.
	String write_table_to_string(const Ref<VCSVTable> &p_table);
	int write_table(const Ref<VCSVTable> &p_table, const String &p_path);

	// Rows (Array of PackedStringArray) to string / file; optional header row.
	String write_rows_to_string(const Array &p_rows, const PackedStringArray &p_headers = PackedStringArray());
	int write_rows(const Array &p_rows, const String &p_path, const PackedStringArray &p_headers = PackedStringArray());

	// Array of Dictionary rows; column order comes from p_column_order, or from
	// the first dict when empty.
	String from_dicts_to_string(const Array &p_dicts, const PackedStringArray &p_column_order = PackedStringArray());

	// Quotes a single field (no line structure) using current delimiter/quote.
	static String quote_field(const String &p_field);

protected:
	static void _bind_methods();

private:
	vortariscsv::CsvWriteOptions to_core() const;
	int write_with_encoding(const String &p_path, const String &p_content) const;

	String delimiter_ = ",";
	String quote_ = "\"";
	String line_ending_ = "\r\n";
	bool always_quote_ = false;
	bool sanitize_formulas_ = false;
	String encoding_ = "utf8";
};

} // namespace godot
