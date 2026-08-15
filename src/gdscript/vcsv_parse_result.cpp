#include "vcsv_parse_result.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

VCSVParseResult::VCSVParseResult() {}

String VCSVParseResult::as_text() const {
	if (success_) {
		String s = "VCSVParseResult(ok";
		if (!warnings_.is_empty()) {
			s += ", " + String::num_int64(warnings_.size()) + " warning(s)";
		}
		return s + ")";
	}
	String s = "VCSVParseResult(error=" + String::num_int64(error_);
	if (error_line_ > 0) {
		s += " @ line " + String::num_int64(error_line_);
		if (error_column_ > 0) {
			s += ":" + String::num_int64(error_column_);
		}
	}
	if (!message_.is_empty()) {
		s += ": " + message_;
	}
	return s + ")";
}

void VCSVParseResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_success"), &VCSVParseResult::get_success);
	ClassDB::bind_method(D_METHOD("set_success", "value"), &VCSVParseResult::set_success);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "success"), "set_success", "get_success");

	ClassDB::bind_method(D_METHOD("get_error"), &VCSVParseResult::get_error);
	ClassDB::bind_method(D_METHOD("set_error", "value"), &VCSVParseResult::set_error);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "set_error", "get_error");

	ClassDB::bind_method(D_METHOD("get_message"), &VCSVParseResult::get_message);
	ClassDB::bind_method(D_METHOD("set_message", "value"), &VCSVParseResult::set_message);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "set_message", "get_message");

	ClassDB::bind_method(D_METHOD("get_error_line"), &VCSVParseResult::get_error_line);
	ClassDB::bind_method(D_METHOD("set_error_line", "value"), &VCSVParseResult::set_error_line);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error_line"), "set_error_line", "get_error_line");

	ClassDB::bind_method(D_METHOD("get_error_column"), &VCSVParseResult::get_error_column);
	ClassDB::bind_method(D_METHOD("set_error_column", "value"), &VCSVParseResult::set_error_column);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error_column"), "set_error_column", "get_error_column");

	ClassDB::bind_method(D_METHOD("get_warnings"), &VCSVParseResult::get_warnings);
	ClassDB::bind_method(D_METHOD("set_warnings", "value"), &VCSVParseResult::set_warnings);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "warnings"), "set_warnings", "get_warnings");

	ClassDB::bind_method(D_METHOD("get_table"), &VCSVParseResult::get_table);
	ClassDB::bind_method(D_METHOD("set_table", "value"), &VCSVParseResult::set_table);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "table", PROPERTY_HINT_RESOURCE_TYPE, "VCSVTable"), "set_table", "get_table");

	ClassDB::bind_method(D_METHOD("get_column_types"), &VCSVParseResult::get_column_types);
	ClassDB::bind_method(D_METHOD("set_column_types", "value"), &VCSVParseResult::set_column_types);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "column_types"), "set_column_types", "get_column_types");

	ClassDB::bind_method(D_METHOD("ok"), &VCSVParseResult::ok);
	ClassDB::bind_method(D_METHOD("as_text"), &VCSVParseResult::as_text);
}

} // namespace godot
