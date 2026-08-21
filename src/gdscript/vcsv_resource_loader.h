#pragma once

#include <godot_cpp/classes/resource_format_loader.hpp>

namespace godot {

// Native ResourceFormatLoader so `load("*.csv")` / `load("*.tsv")` returns a
// ready VCSVDataTable at RUNTIME — including from mounted PCKs and in exported
// builds, where the editor's .import remap (`.godot/imported/*.tres`) does not
// exist. In the editor, imported Vortaris CSVs still resolve through their
// .import remap first; this loader only serves raw, un-imported files.
//
// Fixes "No loader found for resource: ...csv" in exports (issue #5).
class VCSVResourceLoader : public ResourceFormatLoader {
	GDCLASS(VCSVResourceLoader, ResourceFormatLoader)

public:
	VCSVResourceLoader();

	Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads,
			int32_t p_cache_mode) const override;
	bool _recognize_path(const String &p_path, const StringName &p_type) const override;
	bool _handles_type(const StringName &p_type) const override;
	PackedStringArray _get_recognized_extensions() const override;
	bool _exists(const String &p_path) const override;
	String _get_resource_type(const String &p_path) const override;

protected:
	static void _bind_methods();
};

} // namespace godot
