#pragma once

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "vcsv_settings.h"

namespace vortariscsv {

// Gated logging helpers (mirrors ModLoader's vortarismodloader/verbose +
// log_verbose convention).
//
// Gate matrix:
//   - log_info     : DEBUG_ENABLED only (godot-cpp defines DEBUG_ENABLED for
//                    template_debug and editor targets; NOT for template_release).
//                    Use for normal operational logs (import started/finished,
//                    a file parse completed).
//   - log_verbose  : DEBUG_ENABLED + ProjectSettings "vortariscsv/general/verbose"
//                    == true (0.2.x flat "vortariscsv/verbose" is read as fallback).
//                    Use for detailed logs (parse internals, data-table rebuild,
//                    hot reload, validation progress).
//   - Errors / warnings are NOT gated: always push_error / push_warning directly.
//
// The "vortariscsv/general/verbose" setting is registered by the editor plugin
// GDScript (demo/addons/vortariscsv/editor_plugin.gd) so it appears in Project
// Settings.
//
// NOTE: godot types are used fully qualified (godot::String, ...) so this header
// is safe to include from any namespace context (the plugin sources are partly
// in `namespace godot` and partly include this header before that namespace opens).
//
// IMPORTANT: these are MACROS, not functions. The message expression passed to
// VCSV_LOG_INFO / VCSV_LOG_VERBOSE is only evaluated when the gate is open, so
// in release builds (debug_logging_enabled() is a compile-time false) the string
// concatenation / allocation at the call site is compiled out entirely — release
// DLLs carry none of the log literals and pay zero hot-path cost. Call sites MUST
// use the macros; the *_impl functions below are internal.

#ifdef DEBUG_ENABLED
// Compile-time constant: true in debug/editor builds, false in template_release.
// The optimizer folds the surrounding `if` away entirely in release builds.
constexpr bool debug_logging_enabled() {
	return true;
}
#else // !DEBUG_ENABLED (template_release)
constexpr bool debug_logging_enabled() {
	return false;
}
#endif // DEBUG_ENABLED

inline bool verbose_logging_enabled() {
	if (!debug_logging_enabled()) {
		return false;
	}
	// 0.2.x used the flat "vortariscsv/verbose"; 0.3.x+ reads the hierarchical
	// "vortariscsv/general/verbose" with a fallback so existing projects keep
	// their setting.
	return get_bool_setting_with_fallback("vortariscsv/general/verbose", "vortariscsv/verbose", false);
}

inline void log_info_impl(const godot::String &p_msg) {
	godot::UtilityFunctions::print("[vortariscsv] " + p_msg);
}

inline void log_verbose_impl(const godot::String &p_msg) {
	godot::UtilityFunctions::print("[vortariscsv][v] " + p_msg);
}

} // namespace vortariscsv

#define VCSV_LOG_INFO(msg) \
	do { \
		if (vortariscsv::debug_logging_enabled()) { \
			vortariscsv::log_info_impl(msg); \
		} \
	} while (0)

#define VCSV_LOG_VERBOSE(msg) \
	do { \
		if (vortariscsv::verbose_logging_enabled()) { \
			vortariscsv::log_verbose_impl(msg); \
		} \
	} while (0)
