#include "row_types.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

DemoMonsterRow::DemoMonsterRow() {}

void DemoMonsterRow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &DemoMonsterRow::get_id);
	ClassDB::bind_method(D_METHOD("set_id", "v"), &DemoMonsterRow::set_id);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "id"), "set_id", "get_id");

	ClassDB::bind_method(D_METHOD("get_name"), &DemoMonsterRow::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "v"), &DemoMonsterRow::set_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");

	ClassDB::bind_method(D_METHOD("get_health"), &DemoMonsterRow::get_health);
	ClassDB::bind_method(D_METHOD("set_health", "v"), &DemoMonsterRow::set_health);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");

	ClassDB::bind_method(D_METHOD("get_attack"), &DemoMonsterRow::get_attack);
	ClassDB::bind_method(D_METHOD("set_attack", "v"), &DemoMonsterRow::set_attack);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack"), "set_attack", "get_attack");

	ClassDB::bind_method(D_METHOD("get_alive"), &DemoMonsterRow::get_alive);
	ClassDB::bind_method(D_METHOD("set_alive", "v"), &DemoMonsterRow::set_alive);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alive"), "set_alive", "get_alive");

	ClassDB::bind_method(D_METHOD("get_position"), &DemoMonsterRow::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "v"), &DemoMonsterRow::set_position);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");

	ClassDB::bind_method(D_METHOD("get_color"), &DemoMonsterRow::get_color);
	ClassDB::bind_method(D_METHOD("set_color", "v"), &DemoMonsterRow::set_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");

	ClassDB::bind_method(D_METHOD("get_tags"), &DemoMonsterRow::get_tags);
	ClassDB::bind_method(D_METHOD("set_tags", "v"), &DemoMonsterRow::set_tags);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "tags"), "set_tags", "get_tags");
}

} // namespace godot
