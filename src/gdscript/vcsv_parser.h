#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

#include "vcsv_parse_options.h"
#include "vcsv_parse_result.h"

namespace godot {

// Low-level CSV/DSV parser. Turns text (or a file) into a VCSVTable of raw
// strings. No type conversion happens here — see VCSVDataTable for that.
class VCSVParser : public RefCounted {
	GDCLASS(VCSVParser, RefCounted)

public:
	VCSVParser();

	// Parses `p_text` and returns a structured result.
	static Ref<VCSVParseResult> parse_string(const String &p_text, const Ref<VCSVParseOptions> &p_options);
	// Reads the file as bytes (UTF-8), strips a BOM when enabled, then parses.
	static Ref<VCSVParseResult> parse_file(const String &p_path, const Ref<VCSVParseOptions> &p_options);

protected:
	static void _bind_methods();
};

} // namespace godot
