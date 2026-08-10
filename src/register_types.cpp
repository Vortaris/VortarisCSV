#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

// Runtime-facing classes (VCSV prefix). Registered at SCENE level.
#include "gdscript/vcsv_data_table.h"
#include "gdscript/vcsv_parse_options.h"
#include "gdscript/vcsv_parse_result.h"
#include "gdscript/vcsv_parser.h"
#include "gdscript/vcsv_table.h"
#include "gdscript/vcsv_util.h"
#include "gdscript/vcsv_writer.h"

// Demo C++ row type compiled into the extension (for ClassDB binding tests).
#include "demo/row_types.h"

// Editor-only import plugin (registered at EDITOR level; a no-op in templates).
#include "editor/vcsv_editor_import_plugin.h"

using namespace godot;

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
		GDREGISTER_CLASS(DemoMonsterRow);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(VCSVEditorImportPlugin);
	}
}

void uninitialize_vortariscsv_module(ModuleInitializationLevel p_level) {
	// No heap-allocated singletons yet; nothing to tear down.
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
