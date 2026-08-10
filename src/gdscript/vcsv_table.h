#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "../core/column_index.h"

namespace godot {

// The low-level data container: a header row plus a grid of string rows.
// Produced by VCSVParser; consumed by VCSVDataTable (high level).
class VCSVTable : public Resource {
	GDCLASS(VCSVTable, Resource)

public:
	VCSVTable();

	// Replaces headers and data rows (rows must NOT include the header row).
	void set_data(const PackedStringArray &p_headers, const Array &p_rows);

	PackedStringArray get_headers() const { return headers_; }
	Array get_rows() const { return rows_; }

	String get_name() const { return name_; }
	void set_name(const String &p_name) { name_ = p_name; }

	PackedStringArray get_header() const { return headers_; }
	int64_t get_row_count() const;
	int64_t get_col_count() const;
	bool is_empty() const;

	PackedStringArray get_row(int64_t p_index) const;
	// p_col is either an int column index or a String header name.
	String get_value(int64_t p_row, const Variant &p_col) const;
	Dictionary get_row_dict(int64_t p_index) const;
	Array to_dict_array() const;

	bool has_column(const String &p_name) const;
	int64_t column_index(const String &p_name) const;

protected:
	static void _bind_methods();

private:
	void invalidate_index() const;
	mutable vortariscsv::ColumnIndex index_;

	PackedStringArray headers_;
	Array rows_;
	String name_;
};

} // namespace godot
