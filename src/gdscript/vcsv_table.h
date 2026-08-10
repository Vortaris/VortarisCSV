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
	// Cell matching modes used by find / find_first.
	enum MatchMode {
		MATCH_EXACT = 0,
		MATCH_NOCASE_EXACT = 1,
		MATCH_CONTAINS = 2,
		MATCH_NOCASE_CONTAINS = 3,
		MATCH_PREFIX = 4,
		MATCH_NOCASE_PREFIX = 5,
	};

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

	// Sorts the data rows in place by the cell values of `p_column` (an int
	// index or a String header name). When `p_numeric`, cells are compared as
	// numbers (non-numeric cells sort as 0); otherwise lexicographically.
	void sort(const Variant &p_column, bool p_ascending = true, bool p_numeric = false);

	// Returns the indices of rows whose `p_column` cell matches `p_value`
	// per `p_match_mode` (see MatchMode).
	PackedInt32Array find(const Variant &p_column, const String &p_value, int64_t p_match_mode = 0) const;
	// Returns the first matching row index, or -1.
	int64_t find_first(const Variant &p_column, const String &p_value, int64_t p_match_mode = 0) const;
	// Returns the indices of rows for which `p_predicate.call(row_cells)` is
	// truthy (row_cells is the row as a PackedStringArray).
	PackedInt32Array find_where(const Callable &p_predicate) const;

	// Appends a row (elements are stringified). Returns the new row index.
	int64_t add_row(const Array &p_values);
	// Appends many rows at once (Array of PackedStringArray or Array of Arrays).
	void add_rows(const Array &p_rows);
	// Aggregate statistics over a column's cells: {count, non_empty, numeric,
	// min, max, sum, avg, distinct}. min/max/sum/avg only when numeric cells exist.
	Dictionary column_stats(const Variant &p_column) const;
	// Removes the row at `p_index` (shift down). Returns false if out of range.
	bool remove_row(int64_t p_index);
	// Sets one cell; `p_col` is an int index or a String header name.
	void set_cell(int64_t p_row, const Variant &p_col, const String &p_value);
	// Unique cell values of `p_column`, in first-occurrence order.
	PackedStringArray get_distinct(const Variant &p_column) const;
	// All cell values of `p_column` (raw strings, in row order).
	PackedStringArray get_column(const Variant &p_column) const;

	// Serializes this table to a JSON array of dictionaries.
	String to_json_string() const;
	// Builds a table from an Array of Dictionary rows (column order follows
	// `p_column_order`, or the first dict's keys).
	static Ref<VCSVTable> from_dict_array(const Array &p_dicts, const PackedStringArray &p_column_order = PackedStringArray());
	// Builds a table from a JSON array-of-dictionaries string.
	static Ref<VCSVTable> from_json_string(const String &p_json);

protected:
	static void _bind_methods();

private:
	void invalidate_index() const;
	mutable vortariscsv::ColumnIndex index_;
	int64_t resolve_column(const Variant &p_column) const;
	bool cell_matches(const String &p_cell, const String &p_value, int64_t p_match_mode) const;

	PackedStringArray headers_;
	Array rows_;
	String name_;
};

} // namespace godot

// Lets BIND_ENUM_CONSTANT / GDScript see VCSVTable.MATCH_* as enum values.
// Must sit outside the godot namespace (the macro opens its own).
VARIANT_ENUM_CAST(VCSVTable::MatchMode);
