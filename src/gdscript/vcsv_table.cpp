#include "vcsv_table.h"

#include <algorithm>
#include <vector>

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "../core/string_match.h"

namespace godot {

VCSVTable::VCSVTable() {}

void VCSVTable::set_data(const PackedStringArray &p_headers, const Array &p_rows) {
	headers_ = p_headers;
	rows_ = p_rows;
	invalidate_index();
}

int64_t VCSVTable::get_row_count() const {
	return rows_.size();
}

int64_t VCSVTable::get_col_count() const {
	if (!headers_.is_empty()) {
		return headers_.size();
	}
	int64_t max_cols = 0;
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			PackedStringArray row = v;
			if (row.size() > max_cols) {
				max_cols = row.size();
			}
		}
	}
	return max_cols;
}

bool VCSVTable::is_empty() const {
	return rows_.is_empty();
}

PackedStringArray VCSVTable::get_row(int64_t p_index) const {
	if (p_index < 0 || p_index >= rows_.size()) {
		return PackedStringArray();
	}
	const Variant &v = rows_[p_index];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return PackedStringArray();
	}
	return v;
}

String VCSVTable::get_value(int64_t p_row, const Variant &p_col) const {
	if (p_row < 0 || p_row >= rows_.size()) {
		return String();
	}
	int64_t col = -1;
	if (p_col.get_type() == Variant::INT) {
		col = static_cast<int64_t>(p_col);
	} else if (p_col.get_type() == Variant::STRING) {
		col = column_index(p_col);
	} else if (p_col.get_type() == Variant::STRING_NAME) {
		col = column_index(String(p_col));
	}
	if (col < 0) {
		return String();
	}
	const Variant &v = rows_[p_row];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return String();
	}
	PackedStringArray row = v;
	if (col >= row.size()) {
		return String();
	}
	return row[col];
}

Dictionary VCSVTable::get_row_dict(int64_t p_index) const {
	PackedStringArray row = get_row(p_index);
	Dictionary out;
	if (headers_.is_empty()) {
		for (int64_t i = 0; i < row.size(); i++) {
			out[String::num_int64(i)] = row[i];
		}
	} else {
		for (int64_t i = 0; i < row.size(); i++) {
			if (i < headers_.size()) {
				out[headers_[i]] = row[i];
			}
		}
	}
	return out;
}

Array VCSVTable::to_dict_array() const {
	Array out;
	for (int64_t i = 0; i < rows_.size(); i++) {
		out.push_back(get_row_dict(i));
	}
	return out;
}

bool VCSVTable::has_column(const String &p_name) const {
	return column_index(p_name) >= 0;
}

int64_t VCSVTable::column_index(const String &p_name) const {
	if (headers_.is_empty()) {
		return -1;
	}
	if (index_.empty()) {
		index_.rebuild(headers_, false);
	}
	return index_.find(p_name);
}

void VCSVTable::invalidate_index() const {
	index_ = vortariscsv::ColumnIndex();
}

int64_t VCSVTable::resolve_column(const Variant &p_column) const {
	if (p_column.get_type() == Variant::INT) {
		return static_cast<int64_t>(p_column);
	}
	if (p_column.get_type() == Variant::STRING) {
		return column_index(String(p_column));
	}
	if (p_column.get_type() == Variant::STRING_NAME) {
		return column_index(String(p_column));
	}
	return -1;
}

bool VCSVTable::cell_matches(const String &p_cell, const String &p_value, int64_t p_match_mode) const {
	return vortariscsv::cell_matches(p_cell, p_value, p_match_mode);
}

void VCSVTable::sort(const Variant &p_column, bool p_ascending, bool p_numeric) {
	const int64_t col = resolve_column(p_column);
	if (col < 0 || rows_.is_empty()) {
		return;
	}

	std::vector<PackedStringArray> rows;
	rows.reserve((size_t)rows_.size());
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			rows.push_back(PackedStringArray(v));
		}
	}

	const auto cell_at = [col](const PackedStringArray &r) -> String {
		return col < r.size() ? r[col] : String();
	};

	std::stable_sort(rows.begin(), rows.end(), [&](const PackedStringArray &a, const PackedStringArray &b) {
		const String sa = cell_at(a).strip_edges();
		const String sb = cell_at(b).strip_edges();
		int cmp = 0;
		if (p_numeric) {
			const double da = sa.is_valid_float() ? sa.to_float() : 0.0;
			const double db = sb.is_valid_float() ? sb.to_float() : 0.0;
			cmp = (da < db) ? -1 : (da > db) ? 1 : 0;
		} else {
			cmp = (sa < sb) ? -1 : (sa > sb) ? 1 : 0;
		}
		return p_ascending ? cmp < 0 : cmp > 0;
	});

	Array new_rows;
	for (const PackedStringArray &r : rows) {
		new_rows.push_back(r);
	}
	rows_ = new_rows;
	invalidate_index();
}

PackedInt32Array VCSVTable::find_where(const Callable &p_predicate) const {
	PackedInt32Array out;
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		Variant result = p_predicate.call(v);
		if (result.booleanize()) {
			out.push_back((int32_t)i);
		}
	}
	return out;
}

int64_t VCSVTable::add_row(const Array &p_values) {
	PackedStringArray row;
	for (int64_t i = 0; i < p_values.size(); i++) {
		row.push_back(String(p_values[i]));
	}
	rows_.push_back(row);
	invalidate_index();
	return rows_.size() - 1;
}

void VCSVTable::add_rows(const Array &p_rows) {
	for (int64_t i = 0; i < p_rows.size(); i++) {
		const Variant &v = p_rows[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			rows_.push_back(PackedStringArray(v));
		} else if (v.get_type() == Variant::ARRAY) {
			Array arr = v;
			PackedStringArray row;
			for (int64_t c = 0; c < arr.size(); c++) {
				row.push_back(String(arr[c]));
			}
			rows_.push_back(row);
		}
	}
	invalidate_index();
}

Dictionary VCSVTable::column_stats(const Variant &p_column) const {
	Dictionary out;
	const int64_t col = resolve_column(p_column);
	if (col < 0) {
		return out;
	}
	int64_t count = 0;
	int64_t non_empty = 0;
	int64_t numeric_count = 0;
	bool any_numeric = false;
	double sum = 0.0;
	double min_v = 0.0;
	double max_v = 0.0;
	HashMap<String, bool> distinct;

	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		count++;
		if (cell.is_empty()) {
			continue;
		}
		non_empty++;
		distinct[cell] = true;
		const String s = cell.strip_edges();
		if (s.is_valid_float()) {
			const double d = s.to_float();
			if (!any_numeric) {
				any_numeric = true;
				min_v = max_v = d;
			} else {
				if (d < min_v) {
					min_v = d;
				}
				if (d > max_v) {
					max_v = d;
				}
			}
			sum += d;
			numeric_count++;
		}
	}

	out["count"] = count;
	out["non_empty"] = non_empty;
	out["numeric"] = any_numeric;
	out["distinct"] = (int64_t)distinct.size();
	if (any_numeric) {
		const auto num = [](double v) -> Variant {
			return v == static_cast<double>(static_cast<int64_t>(v)) ? Variant((int64_t)v) : Variant(v);
		};
		out["min"] = num(min_v);
		out["max"] = num(max_v);
		out["sum"] = num(sum);
		out["avg"] = numeric_count > 0 ? Variant(sum / (double)numeric_count) : Variant();
	}
	return out;
}

bool VCSVTable::remove_row(int64_t p_index) {
	if (p_index < 0 || p_index >= rows_.size()) {
		return false;
	}
	rows_.remove_at(p_index);
	invalidate_index();
	return true;
}

void VCSVTable::set_cell(int64_t p_row, const Variant &p_col, const String &p_value) {
	if (p_row < 0 || p_row >= rows_.size()) {
		return;
	}
	const int64_t col = resolve_column(p_col);
	if (col < 0) {
		return;
	}
	const Variant &v = rows_[p_row];
	if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
		return;
	}
	PackedStringArray row = v;
	// Pad the row to the requested column if needed.
	while (row.size() <= col) {
		row.push_back(String());
	}
	row[col] = p_value;
	rows_[p_row] = row;
}

PackedStringArray VCSVTable::get_distinct(const Variant &p_column) const {
	PackedStringArray out;
	const int64_t col = resolve_column(p_column);
	if (col < 0) {
		return out;
	}
	HashMap<String, bool> seen;
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		if (cell.is_empty() || seen.has(cell)) {
			continue;
		}
		seen[cell] = true;
		out.push_back(cell);
	}
	return out;
}

PackedStringArray VCSVTable::get_column(const Variant &p_column) const {
	PackedStringArray out;
	const int64_t col = resolve_column(p_column);
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

String VCSVTable::to_json_string() const {
	// sort_keys=false preserves column order (dict insertion order).
	return godot::JSON::stringify(to_dict_array(), String(), false);
}

Ref<VCSVTable> VCSVTable::from_dict_array(const Array &p_dicts, const PackedStringArray &p_column_order) {
	PackedStringArray columns = p_column_order;
	if (columns.is_empty() && p_dicts.size() > 0) {
		const Variant &first = p_dicts[0];
		if (first.get_type() == Variant::DICTIONARY) {
			Array keys = Dictionary(first).keys();
			for (int64_t i = 0; i < keys.size(); i++) {
				columns.push_back(String(keys[i]));
			}
		}
	}

	const auto to_cell = [](const Variant &v) -> String {
		// JSON has no int/float distinction; Godot's JSON.parse returns float
		// for every number. Emit integral floats as ints so they bind back to
		// either int or float cleanly ("100" not "100.0").
		if (v.get_type() == Variant::FLOAT) {
			const double f = static_cast<double>(v);
			if (f == static_cast<double>(static_cast<int64_t>(f)) &&
					f >= -9.2e18 && f <= 9.2e18) {
				return String::num_int64(static_cast<int64_t>(f));
			}
		}
		return String(v);
	};

	Array rows;
	for (int64_t i = 0; i < p_dicts.size(); i++) {
		PackedStringArray row;
		if (p_dicts[i].get_type() == Variant::DICTIONARY) {
			Dictionary d = p_dicts[i];
			for (int64_t c = 0; c < columns.size(); c++) {
				Variant v;
				if (d.has(columns[c])) {
					v = d[columns[c]];
				}
				row.push_back(to_cell(v));
			}
		}
		rows.push_back(row);
	}

	Ref<VCSVTable> table;
	table.instantiate();
	table->set_data(columns, rows);
	return table;
}

Ref<VCSVTable> VCSVTable::from_json_string(const String &p_json) {
	Variant data = godot::JSON::parse_string(p_json);
	if (data.get_type() != Variant::ARRAY) {
		return Ref<VCSVTable>();
	}
	return from_dict_array(Array(data));
}

PackedInt32Array VCSVTable::find(const Variant &p_column, const String &p_value, int64_t p_match_mode) const {
	PackedInt32Array out;
	const int64_t col = resolve_column(p_column);
	if (col < 0) {
		return out;
	}
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		if (vortariscsv::cell_matches(cell, p_value, p_match_mode)) {
			out.push_back((int32_t)i);
		}
	}
	return out;
}

int64_t VCSVTable::find_first(const Variant &p_column, const String &p_value, int64_t p_match_mode) const {
	const int64_t col = resolve_column(p_column);
	if (col < 0) {
		return -1;
	}
	for (int64_t i = 0; i < rows_.size(); i++) {
		const Variant &v = rows_[i];
		if (v.get_type() != Variant::PACKED_STRING_ARRAY) {
			continue;
		}
		PackedStringArray row = v;
		const String cell = col < row.size() ? row[col] : String();
		if (vortariscsv::cell_matches(cell, p_value, p_match_mode)) {
			return i;
		}
	}
	return -1;
}

void VCSVTable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "headers", "rows"), &VCSVTable::set_data);
	ClassDB::bind_method(D_METHOD("get_headers"), &VCSVTable::get_headers);
	ClassDB::bind_method(D_METHOD("get_rows"), &VCSVTable::get_rows);
	ClassDB::bind_method(D_METHOD("get_header"), &VCSVTable::get_header);
	ClassDB::bind_method(D_METHOD("get_name"), &VCSVTable::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &VCSVTable::set_name);
	ClassDB::bind_method(D_METHOD("get_row_count"), &VCSVTable::get_row_count);
	ClassDB::bind_method(D_METHOD("get_col_count"), &VCSVTable::get_col_count);
	ClassDB::bind_method(D_METHOD("is_empty"), &VCSVTable::is_empty);
	ClassDB::bind_method(D_METHOD("get_row", "index"), &VCSVTable::get_row);
	ClassDB::bind_method(D_METHOD("get_value", "row", "col"), &VCSVTable::get_value);
	ClassDB::bind_method(D_METHOD("get_row_dict", "index"), &VCSVTable::get_row_dict);
	ClassDB::bind_method(D_METHOD("to_dict_array"), &VCSVTable::to_dict_array);
	ClassDB::bind_method(D_METHOD("has_column", "name"), &VCSVTable::has_column);
	ClassDB::bind_method(D_METHOD("column_index", "name"), &VCSVTable::column_index);
	ClassDB::bind_method(D_METHOD("sort", "column", "ascending", "numeric"), &VCSVTable::sort,
			DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("find", "column", "value", "match_mode"), &VCSVTable::find, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("find_first", "column", "value", "match_mode"), &VCSVTable::find_first, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("find_where", "predicate"), &VCSVTable::find_where);
	ClassDB::bind_method(D_METHOD("add_row", "values"), &VCSVTable::add_row);
	ClassDB::bind_method(D_METHOD("add_rows", "rows"), &VCSVTable::add_rows);
	ClassDB::bind_method(D_METHOD("column_stats", "column"), &VCSVTable::column_stats);
	ClassDB::bind_method(D_METHOD("remove_row", "index"), &VCSVTable::remove_row);
	ClassDB::bind_method(D_METHOD("set_cell", "row", "col", "value"), &VCSVTable::set_cell);
	ClassDB::bind_method(D_METHOD("get_distinct", "column"), &VCSVTable::get_distinct);
	ClassDB::bind_method(D_METHOD("get_column", "column"), &VCSVTable::get_column);
	ClassDB::bind_method(D_METHOD("to_json_string"), &VCSVTable::to_json_string);
	ClassDB::bind_static_method("VCSVTable", D_METHOD("from_dict_array", "dicts", "column_order"),
			&VCSVTable::from_dict_array, DEFVAL(PackedStringArray()));
	ClassDB::bind_static_method("VCSVTable", D_METHOD("from_json_string", "json"), &VCSVTable::from_json_string);

	BIND_ENUM_CONSTANT(MATCH_EXACT);
	BIND_ENUM_CONSTANT(MATCH_NOCASE_EXACT);
	BIND_ENUM_CONSTANT(MATCH_CONTAINS);
	BIND_ENUM_CONSTANT(MATCH_NOCASE_CONTAINS);
	BIND_ENUM_CONSTANT(MATCH_PREFIX);
	BIND_ENUM_CONSTANT(MATCH_NOCASE_PREFIX);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "headers"), "", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rows"), "", "get_rows");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
}

} // namespace godot
