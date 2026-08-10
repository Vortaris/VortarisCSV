#pragma once

#include <functional>
#include <vector>

#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../core/type_converter.h"

namespace vortariscsv {

// Tuning for reflection binding (filled by VCSVDataTable).
struct BinderContext {
	godot::String array_delimiter = ";";
	godot::String null_token;
	// header name -> canonical type name override (e.g. "id" -> "int").
	godot::Dictionary column_types;
	// Optional user converter: call(column_name, cell, property_dict, row_obj)
	// -> Variant. Returned NIL means "use the built-in conversion".
	godot::Callable user_converter;
	bool case_insensitive_columns = false;
	// Resolves OBJECT-typed cells (foreign keys).
	std::function<godot::Variant(const godot::String &p_cell, const godot::StringName &p_class_name)> object_resolver;
};

// Precomputed header-column -> property mapping for a row type.
class RowLayout {
public:
	RowLayout() = default;

	// Builds the mapping from a prototype instance's property list. Fills
	// r_warnings for properties without a matching column.
	bool build(godot::Object *p_prototype, const godot::PackedStringArray &p_headers,
			const BinderContext &p_ctx, godot::String &r_err, std::vector<godot::String> &r_warnings);

	// Applies one data row onto `p_row`. Non-fatal conversion issues are
	// appended to r_errors as "row:N:col:C: message" strings.
	void bind_row(godot::Object *p_row, const godot::PackedStringArray &p_row_values,
			int64_t p_row_index, const godot::PackedStringArray &p_headers,
			const BinderContext &p_ctx, std::vector<godot::String> &r_errors) const;

	bool valid() const { return !columns_.empty(); }
	// Whether CSV column `p_col` is bound to a property.
	bool has_property_for_column(int64_t p_col) const;
	// The property bound to CSV column `p_col` (empty StringName if none).
	godot::StringName property_for_column(int64_t p_col) const;

private:
	struct ColumnProp {
		bool has_property = false;
		godot::StringName name;
		godot::PropertyInfo info;
	};
	std::vector<ColumnProp> columns_;
};

} // namespace vortariscsv
