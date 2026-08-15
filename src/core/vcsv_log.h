#pragma once

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace vortariscsv {

// Gated logging helpers (mirrors ModLoader's vortarismodloader/verbose +
// log_verbose convention).
//
// Gate matrix:
//   - log_info     : DEBUG_ENABLED only (godot-cpp defines DEBUG_ENABLED for
//                    template_debug and editor targets; NOT for template_release).
//                    Use for normal operational logs (import started/finished,
//                    a file parse completed).
//   - log_verbose  : DEBUG_ENABLED + ProjectSettings "vortariscsv/verbose" == true.
//                    Use for detailed logs (parse internals, data-table rebuild,
//                    hot reload, validation progress).
//   - Errors / warnings are NOT gated: always push_error / push_warning directly.
//
// The "vortariscsv/verbose" setting is registered by the editor plugin GDScript
// (demo/addons/vortariscsv/editor_plugin.gd) so it appears in Project Settings.
//
// NOTE: godot types are used fully qualified (godot::String, ...) so this header
// is safe to include from any namespace context (the plugin sources are partly
// in `namespace godot` and partly include this header before that namespace opens).

#ifdef DEBUG_ENABLED

inline bool verbose_logging_enabled() {
	const godot::ProjectSettings *ps = godot::ProjectSettings::get_singleton();
	if (ps == nullptr) {
		return false;
	}
	return bool(ps->get_setting("vortariscsv/verbose", false));
}

inline void log_info(const godot::String &p_msg) {
	godot::UtilityFunctions::print("[vortariscsv] " + p_msg);
}

inline void log_verbose(const godot::String &p_msg) {
	if (verbose_logging_enabled()) {
		godot::UtilityFunctions::print("[vortariscsv][v] " + p_msg);
	}
}

#else // !DEBUG_ENABLED (template_release)

inline void log_info(const godot::String &p_msg) {
	(void)p_msg; // no-op in release
}

inline void log_verbose(const godot::String &p_msg) {
	(void)p_msg; // no-op in release
}

#endif // DEBUG_ENABLED

} // namespace vortariscsv
