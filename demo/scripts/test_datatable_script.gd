extends SceneTree

# Regression tests for VCSVDataTable reflection binding to a GDScript row type.
# Run: Godot --headless --path demo --script res://scripts/test_datatable_script.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func build_table(src: String) -> VCSVDataTable:
	var r := VCSVParser.parse_string(src, null)
	if not r.success:
		return null
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/monster_row.gd"
	return t


func test_binding() -> void:
	var src := "id,name,health,attack,alive,position,color,tags,notes\n" + \
			"goblin,哥布林,100,1.5,true,\"10,20\",#ff0000,\"1;2;3\",{\"weak\":\"fire\"}\n" + \
			"orc,兽人,80,2.0,false,\"30,40\",#00ff00,\"4;5\",{}\n"
	var t := build_table(src)
	check(t != null, "table built")
	var goblin: MonsterRow = t.get_row("goblin")
	check(goblin != null, "row found by key")
	check(goblin.name == "哥布林", "string property")
	check(goblin.health == 100 and typeof(goblin.health) == TYPE_INT, "int property")
	check(goblin.attack == 1.5, "float property")
	check(goblin.alive == true and typeof(goblin.alive) == TYPE_BOOL, "bool property")
	check(goblin.position == Vector2(10, 20), "Vector2 property")
	check(goblin.color == Color("#ff0000"), "Color property")
	check(goblin.tags == [1, 2, 3], "typed int array property")
	check(typeof(goblin.tags) == TYPE_ARRAY, "tags is Array")
	var notes: Dictionary = goblin.notes
	check(notes["weak"] == "fire", "json Dictionary property")

	var orc: MonsterRow = t.get_row("orc")
	check(orc != null, "second row found")
	check(orc.alive == false, "default false kept")
	check(orc.position == Vector2(30, 40), "orc position")


func test_lookups() -> void:
	var src := "id,name,health\nk1,A,10\nk2,B,20\n"
	var t := build_table(src)
	check(t.has_key("k1"), "has_key true")
	check(not t.has_key("nope"), "has_key false")
	check(t.get_row("nope") == null, "missing key null")
	check(t.get_value("k2", "health") == 20, "get_value by property")
	check(t.get_keys() == PackedStringArray(["k1", "k2"]), "get_keys in row order")
	check(t.get_row_by_index(0).get("name") == "A", "get_row_by_index")
	check(t.get_all_rows().size() == 2, "get_all_rows")
	check(t.row_count() == 2, "row_count")
	check(t.column_count() == 3, "column_count")
	var d := t.get_row_dict("k2")
	check(d["name"] == "B", "get_row_dict string level")


func test_string_level_only() -> void:
	# No row_type: string-level access still works, get_row returns null.
	var r := VCSVParser.parse_string("id,name\nk1,A\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	check(t.get_row("k1") == null, "no row_type -> no objects")
	check(t.get_row_dict("k1") == {"id": "k1", "name": "A"}, "string-level dict works")


func test_errors_and_warnings() -> void:
	# Column with no matching property → warning.
	var src := "id,name,missing_col\nk1,A,99\n"
	var t := build_table(src)
	var row: MonsterRow = t.get_row("k1")
	check(row != null, "row built despite unmatched column")
	check(t.get_last_warnings().size() > 0, "warning about unmatched column")
	var warned := false
	for w in t.get_last_warnings():
		if "missing_col" in w and "no matching property" in w:
			warned = true
	check(warned, "extra-column warning names the column and reason")
	# Bad cell value → recorded error, property keeps default.
	var src2 := "id,health\nk1,notanumber\n"
	var t2 := build_table(src2)
	var r2: MonsterRow = t2.get_row("k1")
	check(r2 != null, "row built despite bad cell")
	check(r2.health == 0, "health keeps default on parse error")
	check(t2.get_last_errors().size() > 0, "parse error recorded")


func test_from_file() -> void:
	var t := VCSVDataTable.from_file("res://data/monsters.csv", null,
			"res://scripts/row_types/monster_row.gd")
	check(t != null, "from_file ok")
	var goblin: MonsterRow = t.get_row("goblin")
	check(goblin != null, "from_file row found")
	check(goblin.health == 100, "from_file health")
	check(t.get_row("missing") == null, "from_file missing key")


func test_typed_array_forms() -> void:
	# Array-typed properties accept JSON array literals, ";"-separated strings,
	# and a mixture of both in the same column.
	var src := "id,ints,strs,mixed\n" + \
			"a,\"[1,2,3]\",\"x;y\",\"1;2\"\n" + \
			"b,4;5,\"[\"\"p\"\",\"\"q\"\"]\",\"[6,7]\"\n"
	var r := VCSVParser.parse_string(src, null)
	check(r.success, "array forms parse")
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/array_row.gd"

	var a: ArrayRow = t.get_row("a")
	check(a != null, "row a built")
	if a:
		check(typeof(a.ints) == TYPE_ARRAY and a.ints == [1, 2, 3], "JSON array -> Array[int]")
		check(a.ints is Array[int], "JSON array typed Array[int]")
		check(a.strs == ["x", "y"], "';' string -> Array[String]")
		check(a.strs is Array[String], "';' string typed Array[String]")
		check(a.mixed == [1, 2], "mixed cell ';' form")

	var b: ArrayRow = t.get_row("b")
	check(b != null, "row b built")
	if b:
		check(b.ints == [4, 5], "';' string -> Array[int]")
		check(b.strs == ["p", "q"], "JSON string array -> Array[String]")
		check(b.mixed == [6, 7], "mixed cell JSON form")

	# User converter returning a native (untyped) Array still binds.
	var t2 := VCSVDataTable.new()
	t2.headers = r.table.headers
	t2.rows = r.table.rows
	t2.key_column = "id"
	t2.row_type = "res://scripts/row_types/array_row.gd"
	t2.cell_converter = func(col, _cell, _prop, _row):
		if col == "ints":
			return [9, 8]
		return null
	var c: ArrayRow = t2.get_row("a")
	check(c != null, "converter row built")
	if c:
		check(c.ints == [9, 8], "converter native Array -> Array[int]")


func _init() -> void:
	test_binding()
	test_lookups()
	test_string_level_only()
	test_errors_and_warnings()
	test_from_file()
	test_typed_array_forms()
	if failures == 0:
		print("test_datatable_script OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_datatable_script FAILED: ", failures, "/", checks, " failed")
		quit(1)
