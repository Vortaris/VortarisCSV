#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "vcsv_parse_options.h"
#include "vcsv_parse_result.h"
#include "vcsv_table.h"

namespace godot {

// Static convenience helpers built on VCSVParser / type inference.
class VCSVUtil : public RefCounted {
	GDCLASS(VCSVUtil, RefCounted)

public:
	VCSVUtil();

	// Equivalents of VCSVParser.parse_string / parse_file.
	static Ref<VCSVParseResult> parse(const String &p_text, const Ref<VCSVParseOptions> &p_options);
	static Ref<VCSVParseResult> parse_file(const String &p_path, const Ref<VCSVParseOptions> &p_options);

	// Infers a canonical type name per column (header -> "int"/"float"/...).
	static Dictionary detect_types(const Ref<VCSVTable> &p_table, const String &p_array_delimiter = ";",
			bool p_detect_booleans = false);

	// CSVAccess-style one-liner: parse a CSV and return Array[Dictionary] with
	// cells converted by inferred column types.
	static Array load_csv_dict_array(const String &p_csv_path, const Ref<VCSVParseOptions> &p_options = nullptr);
	// Single-row variant of [load_csv_dict_array]: returns the first row as a
	// Dictionary (typed cells), or an empty Dictionary on failure / empty file.
	static Dictionary load_csv_dict(const String &p_csv_path, const Ref<VCSVParseOptions> &p_options = nullptr);
	// Converts a string table into Array[Dictionary] with inferred cell types.
	// `p_explicit_types` (header name -> canonical type name) overrides inference
	// (used by the header-schema feature).
	static Array table_to_dict_array(const Ref<VCSVTable> &p_table, const String &p_array_delimiter = ";",
			const Dictionary &p_explicit_types = Dictionary());

	// Canonical type name of a Variant value ("int", "Vector2", "string", ...).
	static String type_name(const Variant &p_value);

protected:
	static void _bind_methods();
};

} // namespace godot
