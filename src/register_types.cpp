#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

// Runtime-facing classes (VCSV prefix). Registered at SCENE level.
#include "gdscript/vcsv_data_table.h"
#include "gdscript/vcsv_parse_options.h"
#include "gdscript/vcsv_parse_result.h"
#include "gdscript/vcsv_parser.h"
#include "gdscript/vcsv_resource_loader.h"
#include "gdscript/vcsv_table.h"
#include "gdscript/vcsv_util.h"
#include "gdscript/vcsv_writer.h"

// Demo C++ row type compiled into the extension (for ClassDB binding tests).
#include "demo/row_types.h"

// Editor-only import plugin (registered at EDITOR level; a no-op in templates).
#include "editor/vcsv_editor_import_plugin.h"

using namespace godot;

// Runtime ResourceFormatLoader for .csv/.tsv (issue #5): makes
// `load("*.csv")` work in exported builds and from mounted PCKs, where the
// editor's .import remap does not exist. Kept alive for the module's lifetime.
static Ref<VCSVResourceLoader> g_csv_loader;

void initialize_vortariscsv_module(ModuleInitializationLevel p_level) {
	// Runtime-facing classes (VCSV prefix) are registered at SCENE level.
	// Editor-only classes are registered at EDITOR level.
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(VCSVDataTable);
		GDREGISTER_CLASS(VCSVParseOptions);
		GDREGISTER_CLASS(VCSVParseResult);
		GDREGISTER_CLASS(VCSVParser);
		GDREGISTER_CLASS(VCSVTable);
		GDREGISTER_CLASS(VCSVUtil);
		GDREGISTER_CLASS(VCSVWriter);
		GDREGISTER_CLASS(VCSVResourceLoader);
		GDREGISTER_CLASS(DemoMonsterRow);

		// Native loader so raw .csv/.tsv files resolve to a VCSVDataTable at
		// runtime (editor imports still take precedence via their .import remap).
		g_csv_loader = Ref<VCSVResourceLoader>(memnew(VCSVResourceLoader));
		ResourceLoader::get_singleton()->add_resource_format_loader(g_csv_loader);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(VCSVEditorImportPlugin);
	}
}

void uninitialize_vortariscsv_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (g_csv_loader.is_valid()) {
		ResourceLoader::get_singleton()->remove_resource_format_loader(g_csv_loader);
		g_csv_loader = Ref<VCSVResourceLoader>();
	}
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT vortariscsv_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_vortariscsv_module);
	init_obj.register_terminator(uninitialize_vortariscsv_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
} // extern "C"
