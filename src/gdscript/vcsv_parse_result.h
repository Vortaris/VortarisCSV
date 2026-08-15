#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "vcsv_table.h"

namespace godot {

// Structured result of a parse: success flag, an Error code, a human-readable
// message with line/column, non-fatal warnings, and (on success) the table.
class VCSVParseResult : public RefCounted {
	GDCLASS(VCSVParseResult, RefCounted)

public:
	VCSVParseResult();

	bool get_success() const { return success_; }
	void set_success(bool p_value) { success_ = p_value; }

	int get_error() const { return error_; }
	void set_error(int p_value) { error_ = p_value; }

	String get_message() const { return message_; }
	void set_message(const String &p_value) { message_ = p_value; }

	int64_t get_error_line() const { return error_line_; }
	void set_error_line(int64_t p_value) { error_line_ = p_value; }

	int64_t get_error_column() const { return error_column_; }
	void set_error_column(int64_t p_value) { error_column_ = p_value; }

	PackedStringArray get_warnings() const { return warnings_; }
	void set_warnings(const PackedStringArray &p_value) { warnings_ = p_value; }

	Ref<VCSVTable> get_table() const { return table_; }
	void set_table(const Ref<VCSVTable> &p_value) { table_ = p_value; }

	// Explicit column types declared by header annotations when the parse
	// options set header_type_separator (header name -> canonical type name).
	Dictionary get_column_types() const { return column_types_; }
	void set_column_types(const Dictionary &p_value) { column_types_ = p_value; }

	bool ok() const { return success_; }
	String as_text() const;

protected:
	static void _bind_methods();

private:
	bool success_ = false;
	int error_ = 0;
	String message_;
	int64_t error_line_ = 0;
	int64_t error_column_ = 0;
	PackedStringArray warnings_;
	Ref<VCSVTable> table_;
	Dictionary column_types_;
};

} // namespace godot
