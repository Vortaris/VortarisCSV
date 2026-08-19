extends SceneTree

# 0.4.0 regression tests for VCSVGrid (the Excel-style editor grid).
# Verifies data handling, sorting, selection/copy and the edit->data_row
# mapping without needing a rendered window (headless-safe: _draw is skipped).
# Run: godot --headless --path demo --script res://scripts/test_grid.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func make_grid() -> Control:
	var grid_script: GDScript = load("res://addons/vortariscsv/vcsv_grid.gd")
	var grid: Control = grid_script.new()
	root.add_child(grid)
	return grid


func test_set_data_and_clear() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["id", "name", "hp"]), [
		PackedStringArray(["k1", "goblin", "10"]),
		PackedStringArray(["k2", "orc", "20"]),
	])
	check(g._rows.size() == 2, "rows stored")
	check(g._order.size() == 2, "identity order built")
	check(int(g._order[0]) == 0 and int(g._order[1]) == 1, "identity order content")
	check(g._col_w.size() == 3, "column widths initialized")
	g.clear()
	check(g._rows.size() == 0 and g._headers.size() == 0, "clear empties data")
	g.queue_free()


func test_sort_numeric_and_text() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["id", "name", "hp"]), [
		PackedStringArray(["a", "orc", "100"]),
		PackedStringArray(["b", "goblin", "20"]),
		PackedStringArray(["c", "imp", "3"]),
	])
	# Numeric-aware sort: 3 < 20 < 100 (a naive string sort would say 100 < 20).
	g.set_sort(2, true)
	check(g._row_cell(int(g._order[0]), 0) == "c", "numeric sort asc first (hp=3)")
	check(g._row_cell(int(g._order[2]), 0) == "a", "numeric sort asc last (hp=100)")
	g.set_sort(2, false)
	check(g._row_cell(int(g._order[0]), 0) == "a", "numeric sort desc first (hp=100)")
	# Text sort: natural, case-insensitive.
	g.set_sort(1, true)
	check(g._row_cell(int(g._order[0]), 1) == "goblin", "text sort asc first")
	check(g._row_cell(int(g._order[2]), 1) == "orc", "text sort asc last")
	check(g._sort_col == 1, "sort column recorded")
	g.queue_free()


func test_selection_copy_and_rows() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["id", "name"]), [
		PackedStringArray(["k1", "A"]),
		PackedStringArray(["k2", "B"]),
		PackedStringArray(["k3", "C"]),
	])
	# Single cell (selection coords are Vector2i(col, row)).
	g._sel_from = Vector2i(0, 1)
	g._sel_to = Vector2i(0, 1)
	g._has_selection = true
	check(g.get_selected_text() == "k2", "single cell copy")
	# Rectangle spanning two rows x two cols -> TSV.
	g._sel_from = Vector2i(0, 0)
	g._sel_to = Vector2i(1, 1)
	check(g.get_selected_text() == "k1\tA\nk2\tB", "rectangle copy is TSV")
	var sel_rows: Array = g.get_selected_data_rows()
	check(sel_rows.size() == 2, "selected data rows count")
	check(PackedStringArray(sel_rows[1]) == PackedStringArray(["k2", "B"]), "selected row content")
	g.queue_free()


func test_edit_maps_through_sort() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["id", "hp"]), [
		PackedStringArray(["a", "100"]),
		PackedStringArray(["b", "20"]),
		PackedStringArray(["c", "3"]),
	])
	g.set_editable(true)
	# Sort ascending by hp: view row 0 is data row "c".
	g.set_sort(1, true)
	check(g._row_cell(int(g._order[0]), 0) == "c", "precondition: sorted view row 0 = 'c'")
	var emitted: Array = []
	g.cell_edited.connect(func(data_row: int, col: int, text: String):
		emitted.append([data_row, col, text]))
	# Edit the sorted view row 0 via the overlay machinery.
	g._begin_edit(0, 1)
	check(g._edit_view_row == 0 and g._edit_col == 1, "edit session started")
	check(g._editor.text == "3", "edit overlay prefilled with original value")
	g._editor.text = "33"
	g._commit_edit(true, false)
	check(emitted.size() == 1, "cell_edited emitted once")
	check(int(emitted[0][0]) == 2, "cell_edited carries ORIGINAL data row (2 = 'c')")
	check(int(emitted[0][1]) == 1, "cell_edited column")
	check(String(emitted[0][2]) == "33", "cell_edited text")
	# Internal copy updated in place (data row 2, col 1).
	check(g._row_cell(2, 1) == "33", "internal grid copy updated")
	check(g._row_cell(0, 1) == "100", "other rows untouched")
	g.queue_free()


func test_no_edit_when_not_editable() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["id"]), [PackedStringArray(["x"])])
	g.set_editable(false)
	g._begin_edit(0, 0)
	check(g._edit_view_row == -1, "edit refused when not editable")
	g.queue_free()


func test_column_geometry() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(["a", "b", "c"]), [PackedStringArray(["1", "2", "3"])])
	g._col_w[0] = 150
	g._col_w[1] = 70
	check(float(g._col_x_of(0)) == 0.0, "col 0 x")
	check(float(g._col_x_of(1)) == 150.0, "col 1 x follows resized col 0")
	check(float(g._col_x_of(2)) == 220.0, "col 2 x")
	check(g._total_w() > 220.0 + 40.0, "total width includes gutter + cols")
	g.queue_free()


func test_empty_state() -> void:
	var g := make_grid()
	g.set_data(PackedStringArray(), [])
	check(g.get_selected_text() == "", "no selection text when empty")
	check(g.get_selected_data_rows().size() == 0, "no selected rows when empty")
	g.queue_free()


func _init() -> void:
	# Run on the next idle frame so added children get their _ready.
	call_deferred("_run_all")


func _run_all() -> void:
	test_set_data_and_clear()
	test_sort_numeric_and_text()
	test_selection_copy_and_rows()
	test_edit_maps_through_sort()
	test_no_edit_when_not_editable()
	test_column_geometry()
	test_empty_state()
	if failures == 0:
		print("test_grid OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_grid FAILED: ", failures, "/", checks, " failed")
		quit(1)
