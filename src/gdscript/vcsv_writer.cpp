#include "vcsv_writer.h"

#include <vector>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "../core/csv_writer.h"

namespace godot {

VCSVWriter::VCSVWriter() {}

vortariscsv::CsvWriteOptions VCSVWriter::to_core() const {
	vortariscsv::CsvWriteOptions o;
	o.delimiter = delimiter_;
	o.quote = quote_;
	o.line_ending = line_ending_;
	o.always_quote = always_quote_;
	o.sanitize_formulas = sanitize_formulas_;
	return o;
}

namespace {
// Collects rows into a std::vector. Accepts PackedStringArray entries directly,
// or plain Arrays whose elements are stringified (GDScript-friendly).
void rows_to_vector(const Array &p_rows, std::vector<PackedStringArray> &r_out) {
	for (int64_t i = 0; i < p_rows.size(); i++) {
		const Variant &v = p_rows[i];
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			r_out.push_back(PackedStringArray(v));
		} else if (v.get_type() == Variant::ARRAY) {
			Array arr = v;
			PackedStringArray row;
			for (int64_t c = 0; c < arr.size(); c++) {
				row.push_back(String(arr[c]));
			}
			r_out.push_back(row);
		} else {
			r_out.emplace_back();
		}
	}
}

int write_string_to_file(const String &p_path, const String &p_content) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	f->store_string(p_content);
	f->close();
	return OK;
}
} // namespace

String VCSVWriter::write_table_to_string(const Ref<VCSVTable> &p_table) {
	std::vector<PackedStringArray> rows;
	if (p_table.is_valid()) {
		if (!p_table->get_headers().is_empty()) {
			rows.push_back(p_table->get_headers());
		}
		rows_to_vector(p_table->get_rows(), rows);
	}
	vortariscsv::CsvWriteOptions opts = to_core();
	String resolve_error;
	if (!opts.resolve(resolve_error)) {
		return String();
	}
	return vortariscsv::csv_write_rows(rows, opts);
}

int VCSVWriter::write_table(const Ref<VCSVTable> &p_table, const String &p_path) {
	return write_string_to_file(p_path, write_table_to_string(p_table));
}

String VCSVWriter::write_rows_to_string(const Array &p_rows, const PackedStringArray &p_headers) {
	std::vector<PackedStringArray> rows;
	if (!p_headers.is_empty()) {
		rows.push_back(p_headers);
	}
	rows_to_vector(p_rows, rows);
	vortariscsv::CsvWriteOptions opts = to_core();
	String resolve_error;
	if (!opts.resolve(resolve_error)) {
		return String();
	}
	return vortariscsv::csv_write_rows(rows, opts);
}

int VCSVWriter::write_rows(const Array &p_rows, const String &p_path, const PackedStringArray &p_headers) {
	return write_string_to_file(p_path, write_rows_to_string(p_rows, p_headers));
}

String VCSVWriter::from_dicts_to_string(const Array &p_dicts, const PackedStringArray &p_column_order) {
	PackedStringArray columns = p_column_order;
	if (columns.is_empty() && p_dicts.size() > 0) {
		const Variant &first = p_dicts[0];
		if (first.get_type() == Variant::DICTIONARY) {
			Dictionary d = first;
			Array keys = d.keys();
			for (int64_t i = 0; i < keys.size(); i++) {
				columns.push_back(String(keys[i]));
			}
		}
	}

	std::vector<PackedStringArray> rows;
	if (!columns.is_empty()) {
		rows.push_back(columns);
	}
	for (int64_t i = 0; i < p_dicts.size(); i++) {
		PackedStringArray row;
		if (p_dicts[i].get_type() == Variant::DICTIONARY) {
			Dictionary d = p_dicts[i];
			for (int64_t c = 0; c < columns.size(); c++) {
				Variant v;
				if (d.has(columns[c])) {
					v = d[columns[c]];
				}
				row.push_back(String(v));
			}
		}
		rows.push_back(row);
	}

	vortariscsv::CsvWriteOptions opts = to_core();
	String resolve_error;
	if (!opts.resolve(resolve_error)) {
		return String();
	}
	return vortariscsv::csv_write_rows(rows, opts);
}

String VCSVWriter::quote_field(const String &p_field) {
	// Default options (comma delimiter, double quote, CRLF). No instance is
	// created: stack-constructing a Godot object without memnew() is invalid.
	vortariscsv::CsvWriteOptions opts;
	std::vector<PackedStringArray> rows;
	PackedStringArray single;
	single.push_back(p_field);
	rows.push_back(single);
	String s = vortariscsv::csv_write_rows(rows, opts);
	// Strip the trailing line ending — a single field has no delimiter, so the
	// quoted field is the whole line minus the ending.
	return s.substr(0, s.length() - opts.line_ending.length());
}

void VCSVWriter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_delimiter"), &VCSVWriter::get_delimiter);
	ClassDB::bind_method(D_METHOD("set_delimiter", "value"), &VCSVWriter::set_delimiter);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter"), "set_delimiter", "get_delimiter");

	ClassDB::bind_method(D_METHOD("get_quote"), &VCSVWriter::get_quote);
	ClassDB::bind_method(D_METHOD("set_quote", "value"), &VCSVWriter::set_quote);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "quote"), "set_quote", "get_quote");

	ClassDB::bind_method(D_METHOD("get_line_ending"), &VCSVWriter::get_line_ending);
	ClassDB::bind_method(D_METHOD("set_line_ending", "value"), &VCSVWriter::set_line_ending);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "line_ending"), "set_line_ending", "get_line_ending");

	ClassDB::bind_method(D_METHOD("get_always_quote"), &VCSVWriter::get_always_quote);
	ClassDB::bind_method(D_METHOD("set_always_quote", "value"), &VCSVWriter::set_always_quote);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "always_quote"), "set_always_quote", "get_always_quote");

	ClassDB::bind_method(D_METHOD("get_sanitize_formulas"), &VCSVWriter::get_sanitize_formulas);
	ClassDB::bind_method(D_METHOD("set_sanitize_formulas", "value"), &VCSVWriter::set_sanitize_formulas);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sanitize_formulas"), "set_sanitize_formulas", "get_sanitize_formulas");

	ClassDB::bind_method(D_METHOD("write_table_to_string", "table"), &VCSVWriter::write_table_to_string);
	ClassDB::bind_method(D_METHOD("write_table", "table", "path"), &VCSVWriter::write_table);
	ClassDB::bind_method(D_METHOD("write_rows_to_string", "rows", "headers"), &VCSVWriter::write_rows_to_string, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("write_rows", "rows", "path", "headers"), &VCSVWriter::write_rows, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("from_dicts_to_string", "dicts", "column_order"), &VCSVWriter::from_dicts_to_string, DEFVAL(PackedStringArray()));
	ClassDB::bind_static_method("VCSVWriter", D_METHOD("quote_field", "field"), &VCSVWriter::quote_field);
}

} // namespace godot
