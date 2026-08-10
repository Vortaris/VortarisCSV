#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

// Demo C++ row type, compiled into the extension so VCSVDataTable's ClassDB
// binding path can be tested (see test_datatable_cpp.gd). Users define their
// own row types in their own extensions; this one is for verification.
class DemoMonsterRow : public Resource {
	GDCLASS(DemoMonsterRow, Resource)

public:
	DemoMonsterRow();

	String get_id() const { return id; }
	void set_id(const String &p_v) { id = p_v; }
	String get_name() const { return name; }
	void set_name(const String &p_v) { name = p_v; }
	int64_t get_health() const { return health; }
	void set_health(int64_t p_v) { health = p_v; }
	double get_attack() const { return attack; }
	void set_attack(double p_v) { attack = p_v; }
	bool get_alive() const { return alive; }
	void set_alive(bool p_v) { alive = p_v; }
	Vector2 get_position() const { return position; }
	void set_position(const Vector2 &p_v) { position = p_v; }
	Color get_color() const { return color; }
	void set_color(const Color &p_v) { color = p_v; }
	Array get_tags() const { return tags; }
	void set_tags(const Array &p_v) { tags = p_v; }

protected:
	static void _bind_methods();

private:
	String id;
	String name;
	int64_t health = 0;
	double attack = 0.0;
	bool alive = false;
	Vector2 position;
	Color color = Color(1, 1, 1, 1);
	Array tags;
};

} // namespace godot
