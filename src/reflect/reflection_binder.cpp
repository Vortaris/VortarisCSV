#include "reflection_binder.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace vortariscsv {

using godot::Array;
using godot::Dictionary;
using godot::Object;
using godot::PackedStringArray;
using godot::PROPERTY_USAGE_CATEGORY;
using godot::PROPERTY_USAGE_GROUP;
using godot::PROPERTY_USAGE_INTERNAL;
using godot::PROPERTY_USAGE_SCRIPT_VARIABLE;
using godot::PROPERTY_USAGE_STORAGE;
using godot::PROPERTY_USAGE_SUBGROUP;
using godot::PropertyInfo;
using godot::String;
using godot::StringName;
using godot::Variant;

namespace {

bool property_is_bindable(const PropertyInfo &p_info) {
	const uint32_t usage = p_info.usage;
	if ((usage & PROPERTY_USAGE_STORAGE) == 0 && (usage & PROPERTY_USAGE_SCRIPT_VARIABLE) == 0) {
		return false;
	}
	if (usage & (PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_SUBGROUP)) {
		return false;
	}
	if (p_info.type == Variant::NIL) {
		return false;
	}
	const String name = String(p_info.name);
	if (name == "resource_name" || name == "resource_path" ||
			name == "resource_local_to_scene" || name == "script") {
		return false;
	}
	return true;
}

Dictionary property_to_dict(const PropertyInfo &p_info) {
	Dictionary d;
	d["name"] = p_info.name;
	d["class_name"] = p_info.class_name;
	d["type"] = (int64_t)p_info.type;
	d["hint"] = (int64_t)p_info.hint;
	d["hint_string"] = p_info.hint_string;
	d["usage"] = (int64_t)p_info.usage;
	return d;
}

} // namespace

bool RowLayout::build(Object *p_prototype, const PackedStringArray &p_headers,
		const BinderContext &p_ctx, String &r_err, std::vector<String> &r_warnings) {
	r_err = String();
	columns_.clear();
	columns_.resize((size_t)p_headers.size());

	Array props = p_prototype->get_property_list();
	for (int64_t i = 0; i < props.size(); i++) {
		Dictionary d = props[i];
		PropertyInfo info = PropertyInfo::from_dict(d);
		if (!property_is_bindable(info)) {
			continue;
		}
		const String prop_name = String(info.name);

		int64_t col = -1;
		for (int64_t c = 0; c < p_headers.size(); c++) {
			if (p_headers[c] == prop_name) {
				col = c;
				break;
			}
			if (p_ctx.case_insensitive_columns && p_headers[c].to_lower() == prop_name.to_lower()) {
				col = c;
				break;
			}
		}
		if (col < 0) {
			r_warnings.push_back("property '" + prop_name + "' has no matching CSV column; left at default");
			continue;
		}
		columns_[(size_t)col].has_property = true;
		columns_[(size_t)col].name = info.name;
		columns_[(size_t)col].info = info;
	}

	return true;
}

void RowLayout::bind_row(Object *p_row, const PackedStringArray &p_row_values,
		int64_t p_row_index, const PackedStringArray &p_headers,
		const BinderContext &p_ctx, std::vector<String> &r_errors) const {
	ConvertContext cc;
	cc.array_delimiter = p_ctx.array_delimiter;
	cc.null_token = p_ctx.null_token;
	cc.object_resolver = p_ctx.object_resolver;

	for (int64_t col = 0; col < (int64_t)columns_.size(); col++) {
		const ColumnProp &cp = columns_[(size_t)col];
		if (!cp.has_property) {
			continue;
		}
		if (col >= p_row_values.size()) {
			continue; // missing cell: keep default
		}
		const String &cell = p_row_values[col];
		if (cell.is_empty() || (!p_ctx.null_token.is_empty() && cell == p_ctx.null_token)) {
			continue; // no value: keep default
		}

		// Resolve the effective target property (column_types override wins).
		PropertyInfo target = cp.info;
		bool force_json = false;
		if (!p_ctx.column_types.is_empty() && col < p_headers.size()) {
			Variant type_v = p_ctx.column_types[p_headers[col]];
			if (type_v.get_type() == Variant::STRING) {
				String type_name = type_v;
				if (type_name == "json") {
					force_json = true;
				} else {
					PropertyInfo override;
					if (property_for_type_name(type_name, override)) {
						target = override;
					}
				}
			}
		}

		Variant value;
		bool converted = false;

		// User-supplied converter first.
		if (p_ctx.user_converter.is_valid()) {
			Dictionary prop_dict = property_to_dict(target);
			Variant user = p_ctx.user_converter.call(p_headers[col], cell, prop_dict, Variant(p_row));
			if (user.get_type() != Variant::NIL) {
				value = user;
				converted = true;
			}
		}

		if (!converted) {
			if (force_json) {
				Variant parsed = godot::JSON::parse_string(cell.strip_edges());
				if (parsed.get_type() != Variant::NIL) {
					value = parsed;
					converted = true;
				} else {
					r_errors.push_back("row:" + String::num_int64(p_row_index + 1) +
							":col:" + String::num_int64(col + 1) + ": invalid JSON cell '" + cell + "'");
					continue;
				}
			} else {
				String err;
				value = parse_to_type(cell, target, cc, err);
				if (value.get_type() == Variant::NIL) {
					if (!err.is_empty()) {
						r_errors.push_back("row:" + String::num_int64(p_row_index + 1) +
								":col:" + String::num_int64(col + 1) + ": " + err);
					}
					continue;
				}
			}
		}

		p_row->set(cp.name, value);
	}
}

} // namespace vortariscsv
