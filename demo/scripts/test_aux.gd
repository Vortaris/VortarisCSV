extends SceneTree

# Regression tests for the auxiliary features: VCSVTable.sort/find,
# VCSVDataTable.sort_rows/find_rows/filter/get_column_values.
# Run: Godot --headless --path demo --script res://scripts/test_aux.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func make_table() -> VCSVTable:
	var r := VCSVParser.parse_string("id,hp,name\nk2,50,b\nk1,100,a\nk3,10,c\n", null)
	return r.table


func test_sort() -> void:
	var t := make_table()
	t.sort("hp", true, true) # numeric ascending
	check(t.get_value(0, "id") == "k3", "numeric asc first k3")
	check(t.get_value(1, "id") == "k2", "numeric asc second k2")
	check(t.get_value(2, "id") == "k1", "numeric asc last k1")

	t.sort("hp", false, true) # numeric descending
	check(t.get_value(0, "id") == "k1", "numeric desc first k1")
	check(t.get_value(2, "id") == "k3", "numeric desc last k3")

	t.sort("name") # lexicographic ascending
	check(t.get_value(0, "id") == "k1", "string asc first (name 'a')")

	# Numeric-aware: "10" < "100" < "50" lexicographically.
	var t2 := make_table()
	t2.sort("hp", true, false)
	check(t2.get_value(0, "id") == "k3" and t2.get_value(1, "id") == "k1",
			"lexicographic order 10,100,50")


func test_find() -> void:
	var t := make_table()
	check(t.find("hp", "100") == PackedInt32Array([1]), "find exact by name")
	check(t.find(0, "k2") == PackedInt32Array([0]), "find exact by index")
	check(t.find_first("name", "a") == 1, "find_first exact (row 1)")
	check(t.find_first("name", "zzz") == -1, "find_first missing -> -1")
	check(t.find("id", "k", VCSVTable.MATCH_PREFIX).size() == 3, "prefix finds all")
	check(t.find("name", "A", VCSVTable.MATCH_NOCASE_EXACT) == PackedInt32Array([1]), "nocase exact")
	check(t.find("id", "2", VCSVTable.MATCH_CONTAINS) == PackedInt32Array([0]), "contains k2")


func test_data_table_aux() -> void:
	var src := "id,hp,name\nk2,50,b\nk1,100,a\nk3,10,c\n"
	var r := VCSVParser.parse_string(src, null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/monster_row.gd"

	check(t.find_rows("hp", "100") == PackedStringArray(["k1"]), "find_rows by hp")
	check(t.find_rows("id", "k", VCSVTable.MATCH_PREFIX).size() == 3, "find_rows prefix")
	check(t.find_first_row("hp", "10") == "k3", "find_first_row returns key")
	check(t.find_first_row("hp", "999") == "", "find_first_row missing -> empty")
	check(t.get_column_values("hp") == ["50", "100", "10"], "get_column_values order")

	# sort_rows reorders the grid; keyed lookups still resolve.
	t.sort_rows("hp", true, true)
	check(t.get_row_dict("k3")["hp"] == "10", "sort keeps k3->10")
	check(t.get_row_dict("k1")["hp"] == "100", "sort keeps k1->100")
	check(t.get_row_by_index(0).get("id") == "k3", "sorted row order")

	# filter: use a table whose columns map to monster_row properties.
	var src2 := "id,health,name\nk2,50,b\nk1,100,a\nk3,10,c\n"
	var r2 := VCSVParser.parse_string(src2, null)
	var t2 := VCSVDataTable.new()
	t2.headers = r2.table.headers
	t2.rows = r2.table.rows
	t2.key_column = "id"
	t2.row_type = "res://scripts/row_types/monster_row.gd"

	var strong = t2.filter(func(row): return row.get("health") > 20)
	check(strong.size() == 2, "filter health>20 count")
	if strong.size() == 2:
		check(strong[0].get("id") == "k2", "filter first match")
		check(strong[1].get("id") == "k1", "filter second match")
	var weak = t2.filter(func(row): return row.health < 20)
	check(weak.size() == 1 and weak[0].get("id") == "k3", "filter weak")


func _init() -> void:
	test_sort()
	test_find()
	test_data_table_aux()
	if failures == 0:
		print("test_aux OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_aux FAILED: ", failures, "/", checks, " failed")
		quit(1)
