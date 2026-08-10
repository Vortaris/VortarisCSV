#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_vortariscsv_module(ModuleInitializationLevel p_level) {
	// Runtime-facing classes (VCSV prefix) are registered at SCENE level.
	// Editor-only classes are registered at EDITOR level.
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// TODO(P1+): GDREGISTER_CLASS(VCSVParser); etc.
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// TODO(P6): GDREGISTER_CLASS(VCSVEditorImportPlugin);
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
