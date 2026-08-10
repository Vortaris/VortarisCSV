#include "row_factory.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

namespace vortariscsv {

using godot::ClassDB;
using godot::Object;
using godot::Ref;
using godot::Resource;
using godot::ResourceLoader;
using godot::Script;
using godot::String;
using godot::StringName;
using godot::Variant;

Ref<Resource> instantiate_row_type(const String &p_row_type, String &r_err) {
	r_err = String();
	if (p_row_type.is_empty()) {
		r_err = "row_type is empty";
		return Ref<Resource>();
	}

	if (p_row_type.begins_with("res://") || p_row_type.ends_with(".gd") || p_row_type.ends_with(".cs")) {
		Ref<Resource> loaded = ResourceLoader::get_singleton()->load(p_row_type);
		if (loaded.is_null()) {
			r_err = "cannot load row script: " + p_row_type;
			return Ref<Resource>();
		}
		Ref<Script> script = loaded;
		if (script.is_null()) {
			r_err = "row_type is not a script: " + p_row_type;
			return Ref<Resource>();
		}
		if (!script->can_instantiate()) {
			r_err = "row script cannot be instantiated (is it abstract?): " + p_row_type;
			return Ref<Resource>();
		}
		Variant instance = script->call("new");
		Object *obj = instance.operator Object *();
		if (obj == nullptr) {
			r_err = "row script new() returned null: " + p_row_type;
			return Ref<Resource>();
		}
		Ref<Resource> row = Object::cast_to<Resource>(obj);
		if (row.is_null()) {
			r_err = "row type must extend Resource (got script " + p_row_type + ")";
			return Ref<Resource>();
		}
		return row;
	}

	// ClassDB class name.
	StringName cn(p_row_type);
	if (!ClassDB::class_exists(cn)) {
		r_err = "no such class registered: " + p_row_type;
		return Ref<Resource>();
	}
	if (!ClassDB::can_instantiate(cn)) {
		r_err = "class is not instantiable (abstract?): " + p_row_type;
		return Ref<Resource>();
	}
	Variant instance = ClassDB::instantiate(cn);
	Object *obj = instance.operator Object *();
	if (obj == nullptr) {
		r_err = "class instantiate returned null: " + p_row_type;
		return Ref<Resource>();
	}
	Ref<Resource> row = Object::cast_to<Resource>(obj);
	if (row.is_null()) {
		r_err = "row type must extend Resource (got class " + p_row_type + ")";
		return Ref<Resource>();
	}
	return row;
}

String row_type_class_name(const String &p_row_type) {
	if (p_row_type.is_empty()) {
		return String();
	}
	// Strip directory path, keep basename, strip known script extensions.
	const int64_t slash = p_row_type.rfind("/");
	const int64_t dot = p_row_type.rfind(".");
	String base = (slash != -1) ? p_row_type.substr(slash + 1) : p_row_type;
	if (dot > slash && dot != -1) {
		const int64_t dot_in_base = dot - (slash + 1); // dot is an absolute index
		const String ext = base.substr(dot_in_base);
		if (ext == ".gd" || ext == ".cs") {
			base = base.substr(0, dot_in_base);
		}
	}
	return base;
}

} // namespace vortariscsv
