#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Instantiates a row object from `p_row_type`:
//  - "res://...gd" / "...cs" → load the Script and call new();
//  - otherwise → treat as a ClassDB-registered class name.
// Row objects MUST extend Resource (they are held as Ref<Resource>). Fills
// r_err on failure and returns a null Ref.
godot::Ref<godot::Resource> instantiate_row_type(const godot::String &p_row_type, godot::String &r_err);

// Returns the simple class name implied by a row type ("res://a/b/monster_row.gd"
// → "monster_row"; "MonsterRow" → "MonsterRow"). Used for foreign-key matching.
godot::String row_type_class_name(const godot::String &p_row_type);

} // namespace vortariscsv
