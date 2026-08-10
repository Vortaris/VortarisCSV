#pragma once

#include <vector>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../reflect/reflection_binder.h"
#include "vcsv_parse_options.h"
#include "vcsv_table.h"

namespace godot {

// High-level, UE-DataTable-style resource. Stores a string grid plus a row-type
// reference and lazily builds typed row objects via reflection. Persisted as a
// .tres (only the string grid + config; row objects are rebuilt from the
// current row_type script on access).
class VCSVDataTable : public Resource {
	GDCLASS(VCSVDataTable, Resource)

public:
	VCSVDataTable();

	// ---- persisted config ----
	PackedStringArray get_headers() const { return headers_; }
	void set_headers(const PackedStringArray &p_value);
	Array get_rows() const { return rows_; }
	void set_rows(const Array &p_value);
	String get_key_column() const { return key_column_; }
	void set_key_column(const String &p_value);
	String get_row_type() const { return row_type_; }
	void set_row_type(const String &p_value);
	Dictionary get_column_types() const { return column_types_; }
	void set_column_types(const Dictionary &p_value);
	bool get_case_insensitive_columns() const { return case_insensitive_columns_; }
	void set_case_insensitive_columns(bool p_value);
	String get_array_delimiter() const { return array_delimiter_; }
	void set_array_delimiter(const String &p_value);
	String get_null_token() const { return null_token_; }
	void set_null_token(const String &p_value);
	Dictionary get_linked_tables() const { return linked_tables_; }
	void set_linked_tables(const Dictionary &p_value);

	// ---- runtime ----
	bool ensure_loaded();
	Ref<Resource> get_row(const String &p_key);
	Ref<Resource> get_row_by_index(int64_t p_index);
	Array get_all_rows();
	PackedStringArray get_keys();
	Dictionary get_row_dict(const String &p_key);
	Variant get_value(const String &p_key, const StringName &p_property);
	bool has_key(const String &p_key);
	int64_t row_count() const;
	int64_t column_count() const;

	void set_cell_converter(const Callable &p_callable);
	Callable get_cell_converter() const { return converter_; }

	void set_linked_table(const String &p_name, const String &p_path);
	void refresh();
	void clear_cache();

	// Sorts the data grid in place by a column (int index or String header
	// name); `numeric` compares cells as numbers. Rebuilds the typed cache.
	void sort_rows(const Variant &p_column, bool p_ascending = true, bool p_numeric = false);
	// Returns the keys of rows whose column cell matches `p_value`
	// (match_mode: VCSVTable::MatchMode). Requires key_column; otherwise row
	// indices (as strings) are returned.
	PackedStringArray find_rows(const Variant &p_column, const String &p_value, int64_t p_match_mode = 0) const;
	// Returns the first matching key (or row-index string), empty if none.
	String find_first_row(const Variant &p_column, const String &p_value, int64_t p_match_mode = 0) const;
	// Returns all cell values of a column (raw strings).
	Array get_column_values(const Variant &p_column) const;
	// Returns typed rows for which `p_predicate.call(row)` is truthy.
	Array filter(const Callable &p_predicate);

	Ref<VCSVTable> to_table() const;
	int to_csv(const String &p_path);
	PackedStringArray get_last_errors() const { return last_errors_; }
	PackedStringArray get_last_warnings() const { return last_warnings_; }

	// Runtime one-shot: parse a file and return a ready VCSVDataTable.
	static Ref<VCSVDataTable> from_file(const String &p_path, const Ref<VCSVParseOptions> &p_options,
			const String &p_row_type);

protected:
	static void _bind_methods();

private:
	void mark_dirty();
	bool rebuild();
	void ensure_index();
	Variant resolve_object(const String &p_cell, const StringName &p_class_name);
	Ref<VCSVDataTable> load_linked_table(const String &p_name);

	// Persisted.
	PackedStringArray headers_;
	Array rows_;
	String key_column_;
	String row_type_;
	Dictionary column_types_;
	bool case_insensitive_columns_ = false;
	String array_delimiter_ = ";";
	String null_token_;
	Dictionary linked_tables_;

	// Runtime.
	Callable converter_;
	std::vector<Ref<Resource>> cache_;
	vortariscsv::RowLayout layout_;
	bool cache_dirty_ = true;
	bool building_ = false;
	HashMap<String, int64_t> key_index_;
	HashMap<String, Ref<VCSVDataTable>> linked_cache_;
	PackedStringArray last_errors_;
	PackedStringArray last_warnings_;
};

} // namespace godot
