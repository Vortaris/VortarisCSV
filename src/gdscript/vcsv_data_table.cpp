#include "vcsv_data_table.h"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include "../core/string_match.h"
#include "../core/type_converter.h"
#include "../reflect/row_factory.h"
#include "vcsv_parser.h"
#include "vcsv_util.h"
#include "vcsv_writer.h"

namespace godot {

using vortariscsv::instantiate_row_type;
using vortariscsv::row_type_class_name;

namespace {
// Normalizes a class name for foreign-key matching: lowercase, underscores
// removed ("monster_row" and "MonsterRow" both become "monsterrow").
String normalize_class_name(const String &p_name) {
	String n = p_name.to_lower();
	return n.replace("_", "");
}

// Serializes an object's bindable properties into a Dictionary (used by joins).
Dictionary object_to_dict(Object *p_obj) {
	Dictionary d;
	if (p_obj == nullptr) {
		return d;
	}
	Array props = p_obj->get_property_list();
	for (int64_t i = 0; i < props.size(); i++) {
		PropertyInfo pi = PropertyInfo::from_dict(props[i]);
		if ((pi.usage & PROPERTY_USAGE_STORAGE) == 0 && (pi.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) == 0) {
			continue;
		}
		if (pi.usage & (PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_SUBGROUP)) {
			continue;
		}
		if (pi.type == Variant::NIL) {
			continue;
		}
		const String n = String(pi.name);
		if (n == "resource_name" || n == "resource_path" || n == "resource_local_to_scene" || n == "script") {
			continue;
		}
		d[n] = p_obj->get(pi.name);
	}
	return d;
}

int64_t data_table_column(const Variant &p_column, const PackedStringArray &p_headers, bool p_case_insensitive) {
	if (p_column.get_type() == Variant::INT) {
		return static_cast<int64_t>(p_column);
	}
	if (p_column.get_type() == Variant::STRING || p_column.get_type() == Variant::STRING_NAME) {
		const String name = String(p_column);
		if (!p_case_insensitive) {
			return p_headers.find(name);
		}
		const String lower = name.to_lower();
		for (int64_t i = 0; i < p_headers.size(); i++) {
			if (p_headers[i].to_lower() == lower) {
				return i;
			}
		}
		return -1;
	}
	return -1;
}
} // namespace

VCSVDataTable::VCSVDataTable() {}

void VCSVDataTable::mark_dirty() {
	cache_dirty_ = true;
	cache_.clear();
	layout_ = vortariscsv::RowLayout();
	key_index_.clear();
	row_to_cache_.clear();
}

void VCSVDataTable::set_headers(const PackedStringArray &p_value) {
	if (headers_ == p_value) {
		return;
	}
	headers_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_rows(const Array &p_value) {
	rows_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_key_column(const String &p_value) {
	key_column_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_row_type(const String &p_value) {
	row_type_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_column_types(const Dictionary &p_value) {
	column_types_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_case_insensitive_columns(bool p_value) {
	case_insensitive_columns_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_array_delimiter(const String &p_value) {
	array_delimiter_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_null_token(const String &p_value) {
	null_token_ = p_value;
	mark_dirty();
}

void VCSVDataTable::set_linked_tables(const Dictionary &p_value) {
	linked_tables_ = p_value;
	linked_cache_.clear();
	mark_dirty();
}

void VCSVDataTable::set_linked_table(const String &p_name, const String &p_path) {
	linked_tables_[p_name] = p_path;
	linked_cache_.erase(p_name);
	mark_dirty();
}

void VCSVDataTable::refresh() {
	mark_dirty();
}

void VCSVDataTable::clear_cache() {
	cache_.clear();
	layout_ = vortariscsv::RowLayout();
	cache_dirty_ = true;
}

void VCSVDataTable::ensure_index() {
	// Rebuild only when the data changed, or when a key column exists but the
	// index was never built. A populated, clean index is O(1) on every call.
	const bool should_build = cache_dirty_ || (!key_column_.is_empty() && key_index_.is_empty());
	if (!should_build) {
		return;
	}
	key_index_.clear();
	if (key_column_.is_empty()) {
		return;
	}
	int64_t key_col = headers_.find(key_column_);
	if (key_col < 0) {
		return;
	}
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		if (key_col < row.size()) {
			String key = row[key_col];
			if (!key.is_empty() && !key_index_.has(key)) {
				key_index_[key] = i;
			}
		}
	}
}

bool VCSVDataTable::rebuild() {
	last_errors_.clear();
	last_warnings_.clear();
	cache_.clear();
	layout_ = vortariscsv::RowLayout();
	row_to_cache_.clear();

	if (row_type_.is_empty()) {
		cache_dirty_ = false;
		build_succeeded_ = true;
		return true; // string-level access only
	}

	// Resolve the row type once; reuse it for every row (no per-row load).
	vortariscsv::RowInstantiator factory;
	String err;
	if (!factory.init(row_type_, err)) {
		last_errors_.push_back(err);
		cache_dirty_ = false;
		build_succeeded_ = false;
		emit_signal("build_failed", err);
		return false;
	}
	Ref<Resource> prototype = factory.instantiate(err);
	if (prototype.is_null()) {
		last_errors_.push_back(err);
		cache_dirty_ = false;
		build_succeeded_ = false;
		emit_signal("build_failed", err);
		return false;
	}

	vortariscsv::BinderContext ctx;
	ctx.array_delimiter = array_delimiter_;
	ctx.null_token = null_token_;
	ctx.column_types = column_types_;
	ctx.user_converter = converter_;
	ctx.case_insensitive_columns = case_insensitive_columns_;
	ctx.object_resolver = [this](const String &p_cell, const StringName &p_class) -> Variant {
		return resolve_object(p_cell, p_class);
	};

	std::vector<String> warnings;
	if (!layout_.build(prototype.ptr(), headers_, ctx, err, warnings)) {
		last_errors_.push_back(err);
		cache_dirty_ = false;
		build_succeeded_ = false;
		emit_signal("build_failed", err);
		return false;
	}
	for (const String &w : warnings) {
		last_warnings_.push_back(w);
	}

	ensure_index();
	row_to_cache_.assign((size_t)rows_.size(), -1);

	int64_t cache_i = 0;
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		row_to_cache_[(size_t)i] = cache_i;
		Ref<Resource> row = factory.instantiate(err);
		if (row.is_null()) {
			last_errors_.push_back(err);
			break;
		}
		std::vector<String> errors;
		layout_.bind_row(row.ptr(), PackedStringArray(v), i, headers_, ctx, errors);
		for (const String &e : errors) {
			last_errors_.push_back(e);
		}
		cache_.push_back(row);
		cache_i++;
	}

	cache_dirty_ = false;
	build_succeeded_ = true;
	// Per-cell conversion errors are recorded but non-fatal: the cache is still
	// usable (failing cells keep their defaults).
	if (!last_errors_.is_empty()) {
		emit_signal("build_failed", last_errors_[0]);
	}
	return true;
}

bool VCSVDataTable::ensure_loaded() {
	if (!cache_dirty_) {
		return build_succeeded_; // never lie about a failed structural build
	}
	if (building_) {
		return false; // re-entrant via foreign key: leave unresolved cells null
	}
	building_ = true;
	const bool ok = rebuild();
	building_ = false;
	return ok;
}

Ref<Resource> VCSVDataTable::get_row(const String &p_key) {
	ensure_index();
	if (!key_index_.has(p_key)) {
		return Ref<Resource>();
	}
	const int64_t idx = key_index_[p_key];
	if (row_type_.is_empty() || !ensure_loaded()) {
		return Ref<Resource>();
	}
	if (idx < 0 || idx >= (int64_t)row_to_cache_.size()) {
		return Ref<Resource>();
	}
	const int64_t cidx = row_to_cache_[(size_t)idx];
	if (cidx < 0 || cidx >= (int64_t)cache_.size()) {
		return Ref<Resource>();
	}
	return cache_[(size_t)cidx];
}

Ref<Resource> VCSVDataTable::get_row_by_index(int64_t p_index) {
	if (row_type_.is_empty() || !ensure_loaded()) {
		return Ref<Resource>();
	}
	if (p_index < 0 || p_index >= (int64_t)cache_.size()) {
		return Ref<Resource>();
	}
	return cache_[(size_t)p_index];
}

Array VCSVDataTable::get_all_rows() {
	Array out;
	if (row_type_.is_empty() || !ensure_loaded()) {
		return out;
	}
	for (const Ref<Resource> &row : cache_) {
		out.push_back(row);
	}
	return out;
}

PackedStringArray VCSVDataTable::get_keys() {
	PackedStringArray out;
	ensure_index();
	if (key_index_.is_empty()) {
		return out;
	}
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		int64_t key_col = headers_.find(key_column_);
		if (key_col >= 0 && key_col < row.size()) {
			String key = row[key_col];
			if (!key.is_empty()) {
				out.push_back(key);
			}
		}
	}
	return out;
}

Dictionary VCSVDataTable::get_row_dict(const String &p_key) {
	Dictionary out;
	ensure_index();
	if (!key_index_.has(p_key)) {
		return out;
	}
	const int64_t idx = key_index_[p_key];
	const Variant &v = rows_[idx];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return out;
	}
	PackedStringArray row = v;
	for (int64_t c = 0; c < row.size(); c++) {
		if (c < headers_.size()) {
			out[headers_[c]] = row[c];
		}
	}
	return out;
}

Variant VCSVDataTable::get_value(const String &p_key, const StringName &p_property) {
	Ref<Resource> row = get_row(p_key);
	if (row.is_null()) {
		return Variant();
	}
	return row->get(p_property);
}

bool VCSVDataTable::has_key(const String &p_key) {
	ensure_index();
	return key_index_.has(p_key);
}

int64_t VCSVDataTable::row_count() const {
	return rows_.size();
}

int64_t VCSVDataTable::column_count() const {
	return headers_.size();
}

void VCSVDataTable::set_cell_converter(const Callable &p_callable) {
	converter_ = p_callable;
	mark_dirty();
}

Ref<VCSVDataTable> VCSVDataTable::load_linked_table(const String &p_name) {
	if (linked_cache_.has(p_name)) {
		return linked_cache_[p_name];
	}
	Ref<VCSVDataTable> table;
	if (linked_tables_.has(p_name)) {
		Variant v = linked_tables_[p_name];
		if (v.get_type() == Variant::STRING || v.get_type() == Variant::STRING_NAME) {
			Ref<Resource> loaded = ResourceLoader::get_singleton()->load(String(v));
			if (loaded.is_valid()) {
				table = loaded;
			}
		}
	}
	linked_cache_[p_name] = table;
	return table;
}

Variant VCSVDataTable::resolve_object(const String &p_cell, const StringName &p_class_name) {
	if (linked_tables_.is_empty()) {
		return Variant();
	}
	Array names = linked_tables_.keys();
	const String target = String(p_class_name);
	for (int64_t i = 0; i < names.size(); i++) {
		const String name = String(names[i]);
		Ref<VCSVDataTable> table = load_linked_table(name);
		if (table.is_null()) {
			continue;
		}
		const String tn = row_type_class_name(table->get_row_type());
		if (tn.is_empty()) {
			continue;
		}
		if (normalize_class_name(tn) == normalize_class_name(target)) {
			Ref<Resource> row = table->get_row(p_cell);
			if (row.is_valid()) {
				return Variant(row.ptr());
			}
		}
	}
	return Variant();
}

void VCSVDataTable::sort_rows(const Variant &p_column, bool p_ascending, bool p_numeric) {
	Ref<VCSVTable> table = to_table();
	table->sort(p_column, p_ascending, p_numeric);
	rows_ = table->get_rows();
	mark_dirty();
}

PackedStringArray VCSVDataTable::find_rows(const Variant &p_column, const String &p_value, int64_t p_match_mode) const {
	PackedStringArray out;
	const int64_t col = data_table_column(p_column, headers_, case_insensitive_columns_);
	if (col < 0) {
		return out;
	}
	const int64_t key_col = key_column_.is_empty() ? -1 : headers_.find(key_column_);
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		if (!vortariscsv::cell_matches(cell, p_value, p_match_mode)) {
			continue;
		}
		if (key_col >= 0 && key_col < row.size()) {
			out.push_back(row[key_col]);
		} else {
			out.push_back(String::num_int64(i));
		}
	}
	return out;
}

String VCSVDataTable::find_first_row(const Variant &p_column, const String &p_value, int64_t p_match_mode) const {
	const int64_t col = data_table_column(p_column, headers_, case_insensitive_columns_);
	if (col < 0) {
		return String();
	}
	const int64_t key_col = key_column_.is_empty() ? -1 : headers_.find(key_column_);
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		if (!vortariscsv::cell_matches(cell, p_value, p_match_mode)) {
			continue;
		}
		if (key_col >= 0 && key_col < row.size()) {
			return row[key_col];
		}
		return String::num_int64(i);
	}
	return String();
}

Array VCSVDataTable::get_column_values(const Variant &p_column) const {
	Array out;
	const int64_t col = data_table_column(p_column, headers_, case_insensitive_columns_);
	if (col < 0) {
		return out;
	}
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		out.push_back(col < row.size() ? row[col] : String());
	}
	return out;
}

PackedStringArray VCSVDataTable::get_distinct(const Variant &p_column) const {
	Ref<VCSVTable> t = to_table();
	return t->get_distinct(p_column);
}

void VCSVDataTable::set_cell_value(const String &p_key, const Variant &p_column, const String &p_value) {
	ensure_index();
	if (!key_index_.has(p_key)) {
		return;
	}
	const int64_t row_idx = key_index_[p_key];
	const int64_t col = data_table_column(p_column, headers_, case_insensitive_columns_);
	if (col < 0) {
		return;
	}
	const Variant &v = rows_[row_idx];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return;
	}
	PackedStringArray row = v;
	while (row.size() <= col) {
		row.push_back(String());
	}
	row[col] = p_value;
	rows_[row_idx] = row;
	mark_dirty();
}

void VCSVDataTable::set_row_dict(const String &p_key, const Dictionary &p_dict) {
	ensure_index();
	if (!key_index_.has(p_key)) {
		return;
	}
	const int64_t row_idx = key_index_[p_key];
	const Variant &v = rows_[row_idx];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return;
	}
	PackedStringArray row = v;
	Array keys = p_dict.keys();
	for (int64_t i = 0; i < keys.size(); i++) {
		const int64_t col = headers_.find(String(keys[i]));
		if (col < 0) {
			continue;
		}
		while (row.size() <= col) {
			row.push_back(String());
		}
		row[col] = String(p_dict[keys[i]]);
	}
	rows_[row_idx] = row;
	mark_dirty();
}

bool VCSVDataTable::remove_row(const String &p_key) {
	ensure_index();
	if (!key_index_.has(p_key)) {
		return false;
	}
	rows_.remove_at(key_index_[p_key]);
	mark_dirty();
	return true;
}

void VCSVDataTable::append_dicts(const Array &p_dicts) {
	Ref<VCSVTable> t = VCSVTable::from_dict_array(p_dicts, headers_);
	if (t.is_null()) {
		return;
	}
	Array new_rows = rows_;
	const Array t_rows = t->get_rows();
	for (int64_t i = 0; i < t_rows.size(); i++) {
		new_rows.push_back(t_rows[i]);
	}
	rows_ = new_rows;
	mark_dirty();
}

Dictionary VCSVDataTable::column_stats(const Variant &p_column) const {
	Ref<VCSVTable> t = to_table();
	return t->column_stats(p_column);
}

Ref<Resource> VCSVDataTable::get_related(const String &p_key, const String &p_table_name) {
	Ref<VCSVDataTable> table = load_linked_table(p_table_name);
	if (table.is_null()) {
		return Ref<Resource>();
	}
	return table->get_row(p_key);
}

Dictionary VCSVDataTable::get_related_dict(const String &p_key, const String &p_table_name) {
	Ref<VCSVDataTable> table = load_linked_table(p_table_name);
	if (table.is_null()) {
		return Dictionary();
	}
	return table->get_row_dict(p_key);
}

Array VCSVDataTable::join_rows(const String &p_table_name) {
	Array out;
	if (row_type_.is_empty() || !ensure_loaded()) {
		return out;
	}
	Ref<VCSVDataTable> linked = load_linked_table(p_table_name);
	if (linked.is_null()) {
		return out;
	}
	const String linked_class = row_type_class_name(linked->get_row_type());
	String prefix = p_table_name;
	prefix += ".";

	for (const Ref<Resource> &row : cache_) {
		Dictionary merged;
		Array props = row->get_property_list();
		for (int64_t i = 0; i < props.size(); i++) {
			PropertyInfo pi = PropertyInfo::from_dict(props[i]);
			if ((pi.usage & PROPERTY_USAGE_STORAGE) == 0 && (pi.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) == 0) {
				continue;
			}
			if (pi.usage & (PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_SUBGROUP)) {
				continue;
			}
			const String n = String(pi.name);
			if (n == "resource_name" || n == "resource_path" || n == "resource_local_to_scene" || n == "script") {
				continue;
			}
			Variant val = row->get(pi.name);
			if (pi.type == Variant::OBJECT && !String(pi.class_name).is_empty() &&
					normalize_class_name(String(pi.class_name)) == normalize_class_name(linked_class)) {
				Object *obj = val.operator Object *();
				if (obj != nullptr) {
					Dictionary related = object_to_dict(obj);
					Array related_keys = related.keys();
					for (int64_t k = 0; k < related_keys.size(); k++) {
						String merged_key = prefix;
						merged_key += String(related_keys[k]);
						merged[merged_key] = related[related_keys[k]];
					}
					continue; // the object itself is replaced by the flattened dict
				}
			}
			merged[n] = val;
		}
		out.push_back(merged);
	}
	return out;
}

PackedStringArray VCSVDataTable::get_column(const Variant &p_column) const {
	Ref<VCSVTable> t = to_table();
	return t->get_column(p_column);
}

Array VCSVDataTable::to_dict_array() {
	if (!row_type_.is_empty() && ensure_loaded()) {
		Array out;
		int64_t cache_i = 0;
		for (int64_t r = 0; r < rows_.size(); r++) {
			const Variant &v = rows_[r];
			if (v.get_type() != Variant::PACKED_STRING_ARRAY || cache_i >= (int64_t)cache_.size()) {
				continue;
			}
			Ref<Resource> row = cache_[(size_t)cache_i++];
			PackedStringArray grid = v;
			Dictionary d;
			for (int64_t c = 0; c < headers_.size(); c++) {
				if (layout_.has_property_for_column(c)) {
					d[headers_[c]] = row->get(layout_.property_for_column(c));
				} else {
					d[headers_[c]] = c < grid.size() ? Variant(grid[c]) : Variant();
				}
			}
			out.push_back(d);
		}
		return out;
	}
	// No usable row_type: infer cell types (same as load_csv_dict_array).
	return VCSVUtil::table_to_dict_array(to_table(), array_delimiter_);
}

String VCSVDataTable::to_json_string() {
	// sort_keys=false preserves column order; full_precision keeps float digits.
	return godot::JSON::stringify(to_dict_array(), String(), false, true);
}

Ref<VCSVDataTable> VCSVDataTable::from_dict_array(const Array &p_dicts, const String &p_row_type) {
	Ref<VCSVTable> t = VCSVTable::from_dict_array(p_dicts);
	if (t.is_null()) {
		return Ref<VCSVDataTable>();
	}
	Ref<VCSVDataTable> table;
	table.instantiate();
	table->set_headers(t->get_headers());
	table->set_rows(t->get_rows());
	table->set_row_type(p_row_type);
	if (!table->get_headers().is_empty() && table->get_key_column().is_empty()) {
		table->set_key_column(table->get_headers()[0]);
	}
	return table;
}

Ref<VCSVDataTable> VCSVDataTable::from_json_string(const String &p_json, const String &p_row_type) {
	Variant data = godot::JSON::parse_string(p_json);
	if (data.get_type() != Variant::ARRAY) {
		return Ref<VCSVDataTable>();
	}
	return from_dict_array(Array(data), p_row_type);
}

Array VCSVDataTable::filter(const Callable &p_predicate) {
	Array out;
	if (!p_predicate.is_valid() || row_type_.is_empty() || !ensure_loaded()) {
		return out;
	}
	for (const Ref<Resource> &row : cache_) {
		Variant result = p_predicate.call(row);
		if (result.booleanize()) {
			out.push_back(row);
		}
	}
	return out;
}

Ref<VCSVTable> VCSVDataTable::to_table() const {
	Ref<VCSVTable> table;
	table.instantiate();
	table->set_data(headers_, rows_);
	table->set_name(get_name());
	return table;
}

int VCSVDataTable::to_csv(const String &p_path) {
	Ref<VCSVTable> table = to_table();
	Ref<VCSVWriter> writer;
	writer.instantiate();
	return writer->write_table(table, p_path);
}

Ref<VCSVDataTable> VCSVDataTable::from_file(const String &p_path, const Ref<VCSVParseOptions> &p_options,
		const String &p_row_type) {
	Ref<VCSVParseResult> r = VCSVParser::parse_file(p_path, p_options);
	if (r.is_null() || !r->get_success()) {
		return Ref<VCSVDataTable>();
	}
	Ref<VCSVDataTable> table;
	table.instantiate();
	table->set_headers(r->get_table()->get_headers());
	table->set_rows(r->get_table()->get_rows());
	table->set_row_type(p_row_type);
	table->last_warnings_ = r->get_warnings(); // surface lenient-mode warnings
	if (table->get_key_column().is_empty() && !table->get_headers().is_empty()) {
		table->set_key_column(table->get_headers()[0]); // default: first column is the key
	}
	return table;
}

void VCSVDataTable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_headers"), &VCSVDataTable::get_headers);
	ClassDB::bind_method(D_METHOD("set_headers", "value"), &VCSVDataTable::set_headers);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "headers"), "set_headers", "get_headers");

	ClassDB::bind_method(D_METHOD("get_rows"), &VCSVDataTable::get_rows);
	ClassDB::bind_method(D_METHOD("set_rows", "value"), &VCSVDataTable::set_rows);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rows"), "set_rows", "get_rows");

	ClassDB::bind_method(D_METHOD("get_key_column"), &VCSVDataTable::get_key_column);
	ClassDB::bind_method(D_METHOD("set_key_column", "value"), &VCSVDataTable::set_key_column);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "key_column"), "set_key_column", "get_key_column");

	ClassDB::bind_method(D_METHOD("get_row_type"), &VCSVDataTable::get_row_type);
	ClassDB::bind_method(D_METHOD("set_row_type", "value"), &VCSVDataTable::set_row_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "row_type"), "set_row_type", "get_row_type");

	ClassDB::bind_method(D_METHOD("get_column_types"), &VCSVDataTable::get_column_types);
	ClassDB::bind_method(D_METHOD("set_column_types", "value"), &VCSVDataTable::set_column_types);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "column_types"), "set_column_types", "get_column_types");

	ClassDB::bind_method(D_METHOD("get_case_insensitive_columns"), &VCSVDataTable::get_case_insensitive_columns);
	ClassDB::bind_method(D_METHOD("set_case_insensitive_columns", "value"), &VCSVDataTable::set_case_insensitive_columns);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "case_insensitive_columns"), "set_case_insensitive_columns", "get_case_insensitive_columns");

	ClassDB::bind_method(D_METHOD("get_array_delimiter"), &VCSVDataTable::get_array_delimiter);
	ClassDB::bind_method(D_METHOD("set_array_delimiter", "value"), &VCSVDataTable::set_array_delimiter);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "array_delimiter"), "set_array_delimiter", "get_array_delimiter");

	ClassDB::bind_method(D_METHOD("get_null_token"), &VCSVDataTable::get_null_token);
	ClassDB::bind_method(D_METHOD("set_null_token", "value"), &VCSVDataTable::set_null_token);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "null_token"), "set_null_token", "get_null_token");

	ClassDB::bind_method(D_METHOD("get_linked_tables"), &VCSVDataTable::get_linked_tables);
	ClassDB::bind_method(D_METHOD("set_linked_tables", "value"), &VCSVDataTable::set_linked_tables);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "linked_tables"), "set_linked_tables", "get_linked_tables");

	ClassDB::bind_method(D_METHOD("ensure_loaded"), &VCSVDataTable::ensure_loaded);
	ClassDB::bind_method(D_METHOD("get_row", "key"), &VCSVDataTable::get_row);
	ClassDB::bind_method(D_METHOD("get_row_by_index", "index"), &VCSVDataTable::get_row_by_index);
	ClassDB::bind_method(D_METHOD("get_all_rows"), &VCSVDataTable::get_all_rows);
	ClassDB::bind_method(D_METHOD("get_keys"), &VCSVDataTable::get_keys);
	ClassDB::bind_method(D_METHOD("get_row_dict", "key"), &VCSVDataTable::get_row_dict);
	ClassDB::bind_method(D_METHOD("get_value", "key", "property"), &VCSVDataTable::get_value);
	ClassDB::bind_method(D_METHOD("has_key", "key"), &VCSVDataTable::has_key);
	ClassDB::bind_method(D_METHOD("row_count"), &VCSVDataTable::row_count);
	ClassDB::bind_method(D_METHOD("column_count"), &VCSVDataTable::column_count);

	ClassDB::bind_method(D_METHOD("get_cell_converter"), &VCSVDataTable::get_cell_converter);
	ClassDB::bind_method(D_METHOD("set_cell_converter", "callable"), &VCSVDataTable::set_cell_converter);
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "cell_converter"), "set_cell_converter", "get_cell_converter");

	ClassDB::bind_method(D_METHOD("set_linked_table", "name", "path"), &VCSVDataTable::set_linked_table);
	ClassDB::bind_method(D_METHOD("refresh"), &VCSVDataTable::refresh);
	ClassDB::bind_method(D_METHOD("clear_cache"), &VCSVDataTable::clear_cache);
	ClassDB::bind_method(D_METHOD("sort_rows", "column", "ascending", "numeric"), &VCSVDataTable::sort_rows,
			DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("find_rows", "column", "value", "match_mode"), &VCSVDataTable::find_rows, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("find_first_row", "column", "value", "match_mode"), &VCSVDataTable::find_first_row, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_column_values", "column"), &VCSVDataTable::get_column_values);
	ClassDB::bind_method(D_METHOD("get_distinct", "column"), &VCSVDataTable::get_distinct);
	ClassDB::bind_method(D_METHOD("set_cell_value", "key", "column", "value"), &VCSVDataTable::set_cell_value);
	ClassDB::bind_method(D_METHOD("set_row_dict", "key", "dict"), &VCSVDataTable::set_row_dict);
	ClassDB::bind_method(D_METHOD("remove_row", "key"), &VCSVDataTable::remove_row);
	ClassDB::bind_method(D_METHOD("append_dicts", "dicts"), &VCSVDataTable::append_dicts);
	ClassDB::bind_method(D_METHOD("column_stats", "column"), &VCSVDataTable::column_stats);
	ClassDB::bind_method(D_METHOD("get_related", "key", "table_name"), &VCSVDataTable::get_related);
	ClassDB::bind_method(D_METHOD("get_related_dict", "key", "table_name"), &VCSVDataTable::get_related_dict);
	ClassDB::bind_method(D_METHOD("join_rows", "table_name"), &VCSVDataTable::join_rows);
	ClassDB::bind_method(D_METHOD("filter", "predicate"), &VCSVDataTable::filter);
	ClassDB::bind_method(D_METHOD("get_column", "column"), &VCSVDataTable::get_column);
	ClassDB::bind_method(D_METHOD("to_dict_array"), &VCSVDataTable::to_dict_array);
	ClassDB::bind_method(D_METHOD("to_json_string"), &VCSVDataTable::to_json_string);
	ClassDB::bind_static_method("VCSVDataTable", D_METHOD("from_dict_array", "dicts", "row_type"),
			&VCSVDataTable::from_dict_array, DEFVAL(String()));
	ClassDB::bind_static_method("VCSVDataTable", D_METHOD("from_json_string", "json", "row_type"),
			&VCSVDataTable::from_json_string, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("to_table"), &VCSVDataTable::to_table);
	ClassDB::bind_method(D_METHOD("to_csv", "path"), &VCSVDataTable::to_csv);
	ClassDB::bind_method(D_METHOD("get_last_errors"), &VCSVDataTable::get_last_errors);
	ClassDB::bind_method(D_METHOD("get_last_warnings"), &VCSVDataTable::get_last_warnings);

	ClassDB::bind_static_method("VCSVDataTable", D_METHOD("from_file", "path", "options", "row_type"),
			&VCSVDataTable::from_file, DEFVAL(Variant()), DEFVAL(String()));

	ADD_SIGNAL(MethodInfo("build_failed", PropertyInfo(Variant::STRING, "message")));
}

} // namespace godot
