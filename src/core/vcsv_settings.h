#pragma once

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace vortariscsv {

// Reads a project setting by its new (hierarchical) path, falling back to an
// old flat path when the new one is not set — backward compatibility with 0.2.x
// projects that already configured "vortariscsv/verbose".
//
// When p_old_path is empty no fallback is attempted.
//
// NOTE: godot types are used fully qualified so this header is safe to include
// from any namespace context (the plugin sources are partly in `namespace godot`
// and partly in `namespace vortariscsv`).
inline godot::Variant get_setting_with_fallback(const godot::String &p_new_path, const godot::String &p_old_path,
		const godot::Variant &p_default) {
	const godot::ProjectSettings *ps = godot::ProjectSettings::get_singleton();
	if (ps == nullptr) {
		return p_default;
	}
	if (ps->has_setting(p_new_path)) {
		return ps->get_setting(p_new_path, p_default);
	}
	if (!p_old_path.is_empty() && ps->has_setting(p_old_path)) {
		return ps->get_setting(p_old_path, p_default);
	}
	return p_default;
}

inline bool get_bool_setting_with_fallback(const godot::String &p_new_path, const godot::String &p_old_path,
		bool p_default) {
	return bool(get_setting_with_fallback(p_new_path, p_old_path, p_default));
}

inline godot::String get_string_setting_with_fallback(const godot::String &p_new_path, const godot::String &p_old_path,
		const godot::String &p_default) {
	return godot::String(get_setting_with_fallback(p_new_path, p_old_path, p_default));
}

inline int64_t get_int_setting_with_fallback(const godot::String &p_new_path, const godot::String &p_old_path,
		int64_t p_default) {
	return int64_t(get_setting_with_fallback(p_new_path, p_old_path, p_default));
}

} // namespace vortariscsv
