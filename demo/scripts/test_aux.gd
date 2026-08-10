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


func test_edit_and_distinct() -> void:
	var t := make_table()
	# add_row / set_cell / remove_row.
	var idx := t.add_row(["k4", "7", "d"])
	check(idx == 3, "add_row returns new index")
	check(t.get_row_count() == 4, "add_row grows table")
	t.set_cell(3, "hp", "77")
	check(t.get_value(3, "hp") == "77", "set_cell by name")
	t.set_cell(3, 0, "k9")
	check(t.get_value(3, "id") == "k9", "set_cell by index")
	check(t.remove_row(3), "remove_row ok")
	check(t.get_row_count() == 3, "remove_row shrinks")
	check(not t.remove_row(99), "remove_row out of range false")

	# get_distinct.
	var d := make_table()
	check(d.get_distinct("name") == PackedStringArray(["b", "a", "c"]), "distinct names")
	var d2 := VCSVParser.parse_string("x\n1\n1\n2\n1\n", null).table
	check(d2.get_distinct(0) == PackedStringArray(["1", "2"]), "distinct numeric strings")

	# find_where (grid-level predicate).
	var t2 := make_table()
	var big := t2.find_where(func(row): return row.size() > 0 and row[1].to_int() > 20)
	check(big == PackedInt32Array([0, 1]), "find_where hp>20 indices")


func test_data_table_edit() -> void:
	var src := "id,hp,name\nk2,50,b\nk1,100,a\nk3,10,c\n"
	var r := VCSVParser.parse_string(src, null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/monster_row.gd"

	check(t.get_distinct("name") == PackedStringArray(["b", "a", "c"]), "data table distinct")

	t.set_cell_value("k2", "hp", "999")
	check(t.get_row_dict("k2")["hp"] == "999", "set_cell_value edits grid")
	check(t.get_row("k2").get("id") == "k2", "set_cell_value keeps row")

	check(t.remove_row("k3"), "data table remove_row")
	check(not t.has_key("k3"), "remove_row removes key")
	check(not t.remove_row("nope"), "remove_row missing key false")
	check(t.row_count() == 2, "row_count after remove")


func _init() -> void:
	test_sort()
	test_find()
	test_data_table_aux()
	test_edit_and_distinct()
	test_data_table_edit()
	if failures == 0:
		print("test_aux OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_aux FAILED: ", failures, "/", checks, " failed")
		quit(1)
