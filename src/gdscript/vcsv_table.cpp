#include "vcsv_table.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>

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

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "headers"), "", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rows"), "", "get_rows");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
}

} // namespace godot
