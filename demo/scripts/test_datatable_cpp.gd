extends SceneTree

# Regression tests for VCSVDataTable binding to a C++ ClassDB row type
# (DemoMonsterRow, compiled into the extension).
# Run: Godot --headless --path demo --script res://scripts/test_datatable_cpp.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func test_cpp_binding() -> void:
	var src := "id,name,health,attack,alive,position,color\n" + \
			"goblin,哥布林,100,1.5,true,\"10,20\",#ff0000\n" + \
			"orc,兽人,80,2.0,false,\"30,40\",#00ff00\n"
	var r := VCSVParser.parse_string(src, null)
	check(r.success, "parse ok")
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "DemoMonsterRow" # C++ class name (not a script path)

	var goblin: DemoMonsterRow = t.get_row("goblin")
	check(goblin != null, "row found for C++ class")
	check(goblin is DemoMonsterRow, "instance is DemoMonsterRow")
	check(goblin.name == "哥布林", "string property")
	check(goblin.health == 100, "int property")
	check(goblin.attack == 1.5, "float property")
	check(goblin.alive == true, "bool property")
	check(goblin.position == Vector2(10, 20), "Vector2 property")
	check(goblin.color == Color("#ff0000"), "Color property")

	var orc: DemoMonsterRow = t.get_row("orc")
	check(orc != null, "second row")
	check(orc.alive == false, "default false")


func test_invalid_row_type() -> void:
	var r := VCSVParser.parse_string("id,x\nk1,1\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "NoSuchClass123"
	check(not t.ensure_loaded(), "unknown row type -> ensure_loaded false")
	check(t.get_last_errors().size() > 0, "error recorded for unknown row type")
	check(t.get_row("k1") == null, "no rows built")


func _init() -> void:
	test_cpp_binding()
	test_invalid_row_type()
	if failures == 0:
		print("test_datatable_cpp OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_datatable_cpp FAILED: ", failures, "/", checks, " failed")
		quit(1)
