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
#include "../reflect/row_factory.h"
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
	String get_source_path() const { return source_path_; }
	void set_source_path(const String &p_value);
	bool get_hot_reload() const { return hot_reload_; }
	void set_hot_reload(bool p_value);
	float get_hot_reload_interval() const { return hot_reload_interval_; }
	void set_hot_reload_interval(float p_value) { hot_reload_interval_ = p_value; }
	// When true, rebuild() only builds the structure (layout + index); typed rows
	// are built on demand by build_row() / get_row() / get_row_by_index(). When
	// false (default) the eager path builds every row up front.
	bool get_lazy_build() const { return lazy_build_; }
	void set_lazy_build(bool p_value);

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
	// Unique cell values of a column, in first-occurrence order.
	PackedStringArray get_distinct(const Variant &p_column) const;
	// Sets one cell of the row identified by `p_key` and rebuilds the cache.
	void set_cell_value(const String &p_key, const Variant &p_column, const String &p_value);
	// Sets several cells of the row identified by `p_key` from a Dictionary
	// (header -> value) and rebuilds the cache.
	void set_row_dict(const String &p_key, const Dictionary &p_dict);
	// Removes the row identified by `p_key`. Returns false if not found.
	bool remove_row(const String &p_key);
	// Appends rows from an Array of Dictionary (aligned to this table's headers).
	void append_dicts(const Array &p_dicts);
	// Aggregate statistics over a column (see VCSVTable.column_stats).
	Dictionary column_stats(const Variant &p_column) const;

	// Cross-table queries (foreign keys resolved through linked_tables).
	// Returns the linked table's typed row for `p_key`.
	Ref<Resource> get_related(const String &p_key, const String &p_table_name);
	// Returns the linked table's row as a string-level Dictionary.
	Dictionary get_related_dict(const String &p_key, const String &p_table_name);
	// Merges each typed row with its resolved related row into one Dictionary
	// (related columns prefixed "<table_name>."), for every row that has an
	// OBJECT property bound to the linked table's row type.
	Array join_rows(const String &p_table_name);
	// Returns typed rows for which `p_predicate.call(row)` is truthy.
	Array filter(const Callable &p_predicate);
	// All cell values of a column (raw strings, in row order).
	PackedStringArray get_column(const Variant &p_column) const;
	// Array[Dictionary] with typed values — from bound row properties when a
	// row_type is set, else inferred cell types.
	Array to_dict_array();
	// JSON string of [method to_dict_array].
	String to_json_string();
	// Builds a table from an Array of Dictionary rows (stringified cells).
	static Ref<VCSVDataTable> from_dict_array(const Array &p_dicts, const String &p_row_type = "");
	// Builds a table from a JSON array-of-dictionaries string.
	static Ref<VCSVDataTable> from_json_string(const String &p_json, const String &p_row_type = "");

	Ref<VCSVTable> to_table() const;
	// Alias for [method to_table] (CSVAccess-style naming).
	Ref<VCSVTable> get_table() const { return to_table(); }
	int to_csv(const String &p_path);
	// Writes only the rows whose key is in `p_keys` (headers + subset) to a CSV.
	int export_rows_to_csv(const PackedStringArray &p_keys, const String &p_path);
	// Writes a single row (headers + one data row) to a CSV.
	int export_row_to_csv(const String &p_key, const String &p_path);
	// Data-integrity validation (see _bind_methods / XML for the options).
	PackedStringArray validate(const Dictionary &p_options = Dictionary());
	// Compares the source file's mtime against the last loaded one; when it
	// changed, re-parses `source_path` and marks the cache dirty. Returns true
	// when a reload happened. No-op unless hot_reload and source_path are set.
	bool poll_hot_reload();
	// Registered hot-reload tables (valid instances; dead entries are pruned).
	static Array get_hot_tables();
	PackedStringArray get_last_errors() const { return last_errors_; }
	PackedStringArray get_last_warnings() const { return last_warnings_; }

	// Runtime one-shot: parse a file and return a ready VCSVDataTable.
	static Ref<VCSVDataTable> from_file(const String &p_path, const Ref<VCSVParseOptions> &p_options,
			const String &p_row_type);
	// Lazy mode: builds (and caches) the typed row at original data-row index
	// `p_index`. Returns null when lazy_build is off, the row is not a data row,
	// or the row_type cannot be instantiated.
	Ref<Resource> build_row(int64_t p_index);

protected:
	static void _bind_methods();

private:
	void mark_dirty();
	bool rebuild();
	void ensure_index();
	Variant resolve_object(const String &p_cell, const StringName &p_class_name);
	Ref<VCSVDataTable> load_linked_table(const String &p_name);
	vortariscsv::BinderContext make_binder_context();

	// Hot-reload registry helpers (store instance IDs so dead tables are safe).
	static void register_hot(VCSVDataTable *p_table);
	static void unregister_hot(VCSVDataTable *p_table);
	static std::vector<uint64_t> s_hot_table_ids;

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
	bool build_succeeded_ = true; // whether the last structural build succeeded
	HashMap<String, int64_t> key_index_;
	std::vector<int64_t> row_to_cache_; // original rows_ index -> compact cache_ index
	HashMap<String, Ref<VCSVDataTable>> linked_cache_;
	PackedStringArray last_errors_;
	PackedStringArray last_warnings_;

	// Hot reload.
	String source_path_;
	bool hot_reload_ = false;
	float hot_reload_interval_ = 0.0f;
	uint64_t last_modified_ = 0;
	uint64_t last_poll_ms_ = 0;

	// Lazy build.
	bool lazy_build_ = false;
	std::vector<Ref<Resource>> lazy_cache_; // lazy mode: index = original row index
	vortariscsv::BinderContext lazy_ctx_;
	vortariscsv::RowInstantiator lazy_factory_; // reused across build_row calls
	bool lazy_factory_ready_ = false;
};

} // namespace godot
