extends SceneTree
## Headless smoke of the CSV main-screen editor logic (no EditorPlugin APIs):
## parse + populate + details, cell-edit write-back (with the process_frame
## reimport guard), and Export CSV / Export Rows. Editor GUI itself can't be
## driven headlessly; this exercises the data path.

const MainScreen := preload("res://addons/vortariscsv/csv_main_screen.gd")

var _fails := 0
var _ms: Control = null

func _check(cond: bool, msg: String) -> void:
	if cond:
		print("  ok: ", msg)
	else:
		_fails += 1
		push_error("FAIL: " + msg)

func _initialize() -> void:
	_cleanup()
	await _run()
	if _fails > 0:
		print("test_editor_gui FAIL: ", _fails, " check(s) failed")
		quit(1)
	else:
		print("test_editor_gui OK")
		quit(0)

func _run() -> void:
	# Copy the demo CSV to user:// so write-back is safe.
	var src := "res://data/monsters.csv"
	var dst := "user://test_editor_gui.csv"
	if FileAccess.file_exists(dst):
		DirAccess.remove_absolute(dst)
	DirAccess.copy_absolute(ProjectSettings.globalize_path(src), ProjectSettings.globalize_path(dst))
	_check(FileAccess.file_exists(dst), "copied monsters.csv to user://")

	_ms = MainScreen.new()
	root.add_child(_ms)
	await process_frame
	await process_frame

	# --- set_source_file ------------------------------------------------------
	_ms.set_source_file(dst)
	_check(_ms._editing_allowed, "editing allowed after load")
	_check(_ms._headers.size() == 9, "headers size == 9 (got %d)" % _ms._headers.size())
	_check(_ms._rows.size() == 2, "rows size == 2 (got %d)" % _ms._rows.size())
	_check(_ms._tree != null and _ms._tree.get_root() != null, "tree populated")
	_check(_ms._tree.are_column_titles_visible(), "column titles visible (first-header fix)")
	_check(_ms._tree.is_root_hidden(), "root hidden (no spurious first row)")
	var root_item: TreeItem = _ms._tree.get_root()
	_check(root_item.get_child_count() == 2, "2 data items rendered (got %d)" % root_item.get_child_count())
	_check(_ms._info_label.text.contains("rows: 2"), "info shows row count")
	_check(_ms._types_label.text.contains("health"), "types shows a column type")
	_check(_ms._issues_label.text.contains("validation"), "validation label set")
	# First column header title.
	_check(_ms._tree.get_column_title(0) == "id", "col0 header == 'id' (got '%s')" % _ms._tree.get_column_title(0))

	# --- cell-edit write-back ---------------------------------------------------
	var item: TreeItem = root_item.get_child(0)
	_check(int(item.get_meta("data_row", -1)) == 0, "item 0 maps to data row 0")
	_simulate_edit(0, 1, "goblin2")
	await process_frame
	await process_frame
	await process_frame
	_check(_ms._reimport_pending == false, "reimport guard resets after frame")
	_check(_ms._rows[0][1] == "goblin2", "in-memory row updated")
	var reload := VCSVParser.parse_file(dst, null)
	_check(reload != null and reload.success, "re-parsed file OK")
	_check(reload.table.rows[0][1] == "goblin2", "written back to source csv")

	# --- Export CSV -------------------------------------------------------------
	var export_path := "user://test_editor_gui_export.csv"
	if FileAccess.file_exists(export_path):
		DirAccess.remove_absolute(export_path)
	_ms._export_csv_to(export_path)
	var ex := VCSVParser.parse_file(export_path, null)
	_check(ex != null and ex.success and ex.table.rows[0][1] == "goblin2", "export CSV re-parses with edited cell")

	# --- Export Rows (selected) -------------------------------------------------
	var rows_path := "user://test_editor_gui_rows.csv"
	if FileAccess.file_exists(rows_path):
		DirAccess.remove_absolute(rows_path)
	_ms._tree.set_selected(root_item.get_child(1), 0)
	var sel: Array = [_ms._rows[1]]
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var err := w.write_rows(sel, rows_path, _ms._headers)
	_check(err == OK, "export rows write OK")
	var rx := VCSVParser.parse_file(rows_path, null)
	_check(rx != null and rx.success and rx.table.get_row_count() == 1, "export rows has 1 data row")

	# --- empty / non-csv --------------------------------------------------------
	_ms.set_source_file("")
	_check(_ms._editing_allowed == false, "clears editing on empty path")

	_ms.queue_free()
	await process_frame
	_cleanup()

## Mirrors csv_main_screen._on_item_edited: mutate the in-memory grid cell, then
## write back. (The real signal path is identical but needs a real Tree edit.)
func _simulate_edit(row: int, col: int, value: String) -> void:
	var r: PackedStringArray = _ms._rows[row]
	while r.size() <= col:
		r.append("")
	r[col] = value
	_ms._rows[row] = r
	_ms._write_back()

func _cleanup() -> void:
	for p in ["user://test_editor_gui.csv", "user://test_editor_gui_export.csv", "user://test_editor_gui_rows.csv"]:
		if FileAccess.file_exists(p):
			DirAccess.remove_absolute(p)
