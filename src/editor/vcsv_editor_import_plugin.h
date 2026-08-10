#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Editor import plugin: imports .csv/.tsv files as VCSVDataTable resources
// (.tres). Registered at MODULE_INITIALIZATION_LEVEL_EDITOR and wired up by
// demo/addons/vortariscsv/editor_plugin.gd.
class VCSVEditorImportPlugin : public EditorImportPlugin {
	GDCLASS(VCSVEditorImportPlugin, EditorImportPlugin)

public:
	VCSVEditorImportPlugin();

	String _get_importer_name() const override;
	String _get_visible_name() const override;
	int32_t _get_preset_count() const override;
	String _get_preset_name(int32_t p_preset_index) const override;
	PackedStringArray _get_recognized_extensions() const override;
	TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	float _get_priority() const override;
	int32_t _get_import_order() const override;
	bool _get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const override;
	Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;
	bool _can_import_threaded() const override;

protected:
	static void _bind_methods();
};

} // namespace godot
