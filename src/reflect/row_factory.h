#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/string.hpp>

namespace vortariscsv {

// Instantiates a row object from `p_row_type`:
//  - "res://...gd" / "...cs" → load the Script and call new();
//  - otherwise → treat as a ClassDB-registered class name.
// Row objects MUST extend Resource (they are held as Ref<Resource>). Fills
// r_err on failure and returns a null Ref.
godot::Ref<godot::Resource> instantiate_row_type(const godot::String &p_row_type, godot::String &r_err);

// Reusable row factory: resolves the row_type once (loads the Script / checks
// the class), then creates fresh instances cheaply. Avoids re-loading the
// script for every row when building a large table.
class RowInstantiator {
public:
	RowInstantiator() = default;
	// Resolves the row type. Returns false (with r_err) when it cannot be used.
	bool init(const godot::String &p_row_type, godot::String &r_err);
	// Creates one fresh row instance. Fills r_err on failure.
	godot::Ref<godot::Resource> instantiate(godot::String &r_err);

private:
	bool is_script_path_ = false;
	godot::Ref<godot::Script> script_;
	godot::String class_name_;
};

// Returns the simple class name implied by a row type ("res://a/b/monster_row.gd"
// → "monster_row"; "MonsterRow" → "MonsterRow"). Used for foreign-key matching.
godot::String row_type_class_name(const godot::String &p_row_type);

} // namespace vortariscsv
