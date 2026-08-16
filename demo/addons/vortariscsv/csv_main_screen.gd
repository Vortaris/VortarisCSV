@tool
extends Control
## VortarisCSV editor main screen (the "CSV" tab, next to 2D/3D/Script).
##
## Replaces the old right-dock preview (`editor_table_preview.gd`) with a full
## main-screen workspace, mirroring the VortarisModLoader main-screen pattern:
## a toolbar, a split view (resizable data table | details / validation), clean
## HSeparator / VSeparator rules and a bottom status bar.
##
## Layout:
##   [ toolbar: current file | Import CSV | Export CSV | Export Rows | title ]
##   ---------------------------------------------------------------------------
##   [ HSplitContainer                                              ]
##   [   left:  data table (VCSVResizableTree, draggable columns)   ]
##   [   VSeparator                                                 ]
##   [   right: details (rows/cols/headers/types/validation issues) ]
##   ---------------------------------------------------------------------------
##   [ status bar ]
##
## Double-click a cell to edit it. The change is written back to the source .csv
## with `VCSVWriter`, then the file is reimported on the next `process_frame`
## (one-shot, re-entrancy guarded) — the exact mechanism that was fixed in 0.2.1
## for `editor_table_preview.gd`; it must not regress.

const VCSVResizableTree := preload("res://addons/vortariscsv/vcsv_resizable_tree.gd")

## Render cap: a huge CSV still gets a usable, responsive editor. The note row
## tells the user the rest is kept (and re-saved on edit) even though not shown.
const MAX_ROWS := 1000

var _tree: VCSVResizableTree
var _path_label: Label
var _status: Label
var _info_label: Label
var _headers_label: Label
var _types_label: Label
var _issues_label: Label
var _source_path := ""
var _headers: PackedStringArray = PackedStringArray()
var _rows: Array = []
var _editing_allowed := false
var _pending_path := ""

# Re-entrancy guard for the deferred reimport (see _queue_reimport/_do_reimport).
var _reimport_pending := false
# Path captured at queue time. _do_reimport runs on the next frame, by which time
# _source_path may already point at a different (newly selected) file — always
# reimport the file that was actually edited, not whatever is selected now.
var _reimport_path := ""


func _ready() -> void:
	name = "VortarisCSV"
	# Fill the editor main-screen area: this root is added to the editor's
	# main-screen container, so it needs EXPAND_FILL to take the full height
	# (otherwise it collapses to its minimum and the whole screen looks "short").
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL

	var vbox := VBoxContainer.new()
	vbox.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(vbox)

	# --- Toolbar ------------------------------------------------------------
	var toolbar_panel := PanelContainer.new()
	vbox.add_child(toolbar_panel)
	var toolbar := HBoxContainer.new()
	toolbar_panel.add_child(toolbar)
	_path_label = Label.new()
	_path_label.text = "No .csv selected"
	_path_label.custom_minimum_size = Vector2(280, 0)
	_path_label.clip_text = true
	_path_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_path_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_path_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	toolbar.add_child(_path_label)
	_add_btn(toolbar, "Import CSV", _on_import_csv)
	_add_btn(toolbar, "Export CSV", _on_export_csv)
	_add_btn(toolbar, "Export Rows", _on_export_rows)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	spacer.custom_minimum_size = Vector2(16, 0)
	toolbar.add_child(spacer)
	var title := Label.new()
	title.text = "VortarisCSV"
	title.add_theme_color_override("font_color", Color(0.55, 0.55, 0.6))
	toolbar.add_child(title)

	vbox.add_child(HSeparator.new())

	# --- Split: data table | details ----------------------------------------
	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	vbox.add_child(split)

	# Left: the resizable, editable data table.
	var left_panel := PanelContainer.new()
	left_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.add_child(left_panel)
	var left_vbox := VBoxContainer.new()
	left_panel.add_child(left_vbox)
	left_vbox.add_child(_section_title("Data"))
	left_vbox.add_child(HSeparator.new())
	_tree = VCSVResizableTree.new()
	_tree.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tree.custom_minimum_size = Vector2(0, 220)
	# Multi-row selection feeds "Export Rows" (Ctrl/Cmd-click to pick several).
	_tree.select_mode = Tree.SELECT_MULTI
	_tree.item_edited.connect(_on_item_edited)
	# Editor table font size (project setting vortariscsv/editor/table_font_size).
	var table_font_size := int(ProjectSettings.get_setting("vortariscsv/editor/table_font_size", 14))
	if table_font_size > 0:
		_tree.add_theme_font_size_override("font_size", table_font_size)
	left_vbox.add_child(_tree)

	# A VSeparator between the two panes gives a clear visual boundary; the
	# HSplitContainer keeps the drag handle between the table and the separator,
	# so both stay independently sized.
	split.add_child(VSeparator.new())

	# Right: details + validation.
	var right := VBoxContainer.new()
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.custom_minimum_size = Vector2(300, 0)
	split.add_child(right)
	right.add_child(_section_title("Details"))
	right.add_child(HSeparator.new())
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	right.add_child(scroll)
	var detail_vbox := VBoxContainer.new()
	detail_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(detail_vbox)
	_info_label = Label.new()
	detail_vbox.add_child(_info_label)
	_headers_label = Label.new()
	_headers_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	detail_vbox.add_child(_headers_label)
	_types_label = Label.new()
	_types_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_types_label.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	detail_vbox.add_child(_types_label)
	var issues_scroll := ScrollContainer.new()
	issues_scroll.custom_minimum_size = Vector2(0, 80)
	issues_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	issues_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	detail_vbox.add_child(issues_scroll)
	_issues_label = Label.new()
	_issues_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_issues_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	issues_scroll.add_child(_issues_label)

	# --- Status bar ----------------------------------------------------------
	vbox.add_child(HSeparator.new())
	var status_bar := HBoxContainer.new()
	status_bar.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	status_bar.custom_minimum_size.y = 24
	vbox.add_child(status_bar)
	_status = Label.new()
	_status.clip_text = true
	_status.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_status.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status_bar.add_child(_status)

	# If the plugin pushed a file before we were added to the scene tree
	# (_ready hadn't run), open it now.
	if not _pending_path.is_empty():
		var p := _pending_path
		_pending_path = ""
		set_source_file(p)


## Called by the plugin when the user activates the "CSV" main-screen tab:
## open the currently selected .csv (if any), falling back to the last file.
func on_plugin_tab_shown() -> void:
	if has_meta("plugin_ref"):
		var plugin: EditorPlugin = get_meta("plugin_ref")
		var sel: String = plugin.selected_csv_path()
		if not sel.is_empty():
			set_source_file(sel)
			return
	if not _source_path.is_empty():
		_set_status("file kept: " + _source_path.get_file())


## Set by the plugin when a .csv becomes selected while the tab is hidden.
func set_pending_file(path: String) -> void:
	if not is_node_ready():
		_pending_path = path
		return
	set_source_file(path)


func set_source_file(path: String) -> void:
	_source_path = path
	if path.is_empty() or not path.ends_with(".csv"):
		_path_label.text = "No .csv selected"
		_tree.clear()
		# Do not keep the previous file's data when the path is invalid/cleared.
		_headers = PackedStringArray()
		_rows = []
		_editing_allowed = false
		_info_label.text = ""
		_headers_label.text = ""
		_types_label.text = ""
		_issues_label.text = ""
		_set_status("")
		return
	var r := VCSVParser.parse_file(path, null)
	if r == null or not r.success:
		_path_label.text = path
		_tree.clear()
		# Parse failure must not leave the previous file's grid in memory.
		_headers = PackedStringArray()
		_rows = []
		_editing_allowed = false
		var msg := "parse failed"
		if r != null and not r.message.is_empty():
			msg = r.message
		_info_label.text = ""
		_headers_label.text = ""
		_types_label.text = ""
		_issues_label.text = "error: " + msg
		_set_status("failed to parse " + path.get_file())
		return
	_headers = r.table.headers
	_rows = r.table.rows
	_editing_allowed = true
	_path_label.text = path
	_populate()
	_refresh_details(r)
	_set_status("%d rows x %d cols — %s" % [_rows.size(), _headers.size(), path.get_file()])


# ---------------------------------------------------------------------------
# Table rendering
# ---------------------------------------------------------------------------

func _populate() -> void:
	_tree.clear()
	_setup_columns(_headers)
	var root := _tree.create_item()
	for i in mini(_rows.size(), MAX_ROWS):
		var item := _tree.create_item(root)
		var row: PackedStringArray = _rows[i]
		for c in _headers.size():
			item.set_text(c, row[c] if c < row.size() else "")
			item.set_editable(c, true)
		item.set_meta("data_row", i)
	if _rows.size() > MAX_ROWS:
		var note := _tree.create_item(root)
		note.set_text(0, "... %d more rows (editor caps the view at %d; edits re-save all rows)" % [
			_rows.size() - MAX_ROWS, MAX_ROWS])
		note.set_custom_color(0, Color(0.55, 0.55, 0.6))
		note.set_meta("data_row", -1)


## Columns titles + a sensible default width per column. Only the last column
## expands; every other column is user-draggable and never clips its content.
func _setup_columns(headers: PackedStringArray) -> void:
	_tree.columns = maxi(1, headers.size())
	_tree.set_column_titles_visible(true)
	_tree.hide_root = true
	for c in headers.size():
		var title := str(headers[c])
		_tree.set_column_title(c, title)
		_tree.set_column_expand(c, c == headers.size() - 1)
		_tree.set_column_custom_minimum_width(c, 110)
		_tree.set_column_clip_content(c, false)


# ---------------------------------------------------------------------------
# Details / validation
# ---------------------------------------------------------------------------

func _refresh_details(r) -> void:
	var table = r.table
	_info_label.text = "rows: %d   cols: %d" % [table.get_row_count(), table.get_col_count()]

	var header_parts: PackedStringArray = []
	for h in table.headers:
		header_parts.append(str(h))
	_headers_label.text = "headers: " + ", ".join(header_parts)

	var types: Dictionary = {}
	if ClassDB.class_exists("VCSVUtil"):
		types = VCSVUtil.detect_types(table)
	var type_lines: PackedStringArray = []
	for h in table.headers:
		type_lines.append("  %s: %s" % [h, str(types.get(str(h), "string"))])
	_types_label.text = "inferred column types:\n" + "\n".join(type_lines)

	var issues: PackedStringArray = []
	# Structural checks the C++ validate() doesn't cover for a raw grid.
	var ncols := int(table.get_col_count())
	var rows: Array = table.rows
	for i in rows.size():
		if rows[i].size() != ncols:
			issues.append("row %d has %d cells, expected %d" % [i + 1, rows[i].size(), ncols])
	var seen := {}
	for h in table.headers:
		var hname := str(h)
		if hname.is_empty():
			issues.append("empty header")
		elif seen.has(hname):
			issues.append("duplicate header: " + hname)
		seen[hname] = true
	# VCSVDataTable.validate() — catches key/type/linked-table issues once the
	# user configures a key column / row type on an imported .tres.
	if ClassDB.class_exists("VCSVDataTable"):
		var dt := VCSVDataTable.new()
		dt.set_headers(table.headers)
		dt.set_rows(table.rows)
		var vissues: PackedStringArray = dt.validate()
		for v in vissues:
			issues.append(String(v))
	if issues.is_empty():
		_issues_label.text = "validation: clean"
		_issues_label.add_theme_color_override("font_color", Color(0.6, 0.9, 0.6))
	else:
		_issues_label.text = "validation:\n- " + "\n- ".join(issues)
		_issues_label.add_theme_color_override("font_color", Color(1, 0.6, 0.4))


# ---------------------------------------------------------------------------
# Cell editing + write-back (ported from editor_table_preview.gd, 0.2.1 fix)
# ---------------------------------------------------------------------------

func _on_item_edited() -> void:
	if not _editing_allowed or _source_path.is_empty():
		return
	var item := _tree.get_edited()
	if item == null:
		return
	var col := _tree.get_edited_column()
	var data_row := int(item.get_meta("data_row", -1))
	if data_row < 0 or data_row >= _rows.size() or col < 0 or col >= _headers.size():
		return
	var value := item.get_text(col)

	# Update the in-memory grid.
	var row: PackedStringArray = _rows[data_row]
	while row.size() <= col:
		row.append("")
	row[col] = value
	_rows[data_row] = row
	_write_back()


func _write_back() -> void:
	# Rebuild the whole CSV (headers + data rows) and write back to the source.
	var all: Array = []
	all.append(_headers)
	for row in _rows:
		all.append(row)
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var err := w.write_rows(all, _source_path, PackedStringArray())
	if err != OK:
		printerr("VortarisCSV editor: failed to write ", _source_path, " (err ", err, ")")
		_set_status("write failed: %s" % error_string(err))
		return
	# Reimport so the imported .tres picks up the change. reimport_files() shows
	# the editor progress dialog; calling it synchronously from an item_edited
	# handler (message-queue flush context) prints a stack of
	# editor/gui/progress_dialog.cpp errors ("Do not use progress dialog while
	# flushing the message queue or using call_deferred()!"). Defer to the next
	# process frame (one-shot, with a re-entrancy guard).
	_queue_reimport()
	_set_status("wrote " + _source_path)
	print("VortarisCSV editor: wrote ", _source_path)


func _queue_reimport() -> void:
	if _reimport_pending:
		return
	_reimport_pending = true
	# Capture the path now: _do_reimport runs on the next idle frame, by which
	# time _source_path may have changed (a different file selected meanwhile).
	_reimport_path = _source_path
	var tree := get_tree()
	if tree == null:
		_reimport_pending = false
		_reimport_path = ""
		return
	if not tree.process_frame.is_connected(_do_reimport):
		tree.process_frame.connect(_do_reimport, CONNECT_ONE_SHOT)


func _do_reimport() -> void:
	_reimport_pending = false
	var path := _reimport_path
	_reimport_path = ""
	if path.is_empty():
		return
	if not has_meta("plugin_ref"):
		return
	var plugin: EditorPlugin = get_meta("plugin_ref")
	var fs := plugin.get_editor_interface().get_resource_filesystem()
	if fs == null:
		return
	fs.reimport_files(PackedStringArray([path]))


# ---------------------------------------------------------------------------
# Toolbar actions
# ---------------------------------------------------------------------------

func _on_import_csv() -> void:
	var fd := FileDialog.new()
	fd.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	fd.access = FileDialog.ACCESS_FILESYSTEM
	fd.exclusive = false # avoid "exclusive child window" clash with other dialogs
	fd.add_filter("*.csv, *.tsv", "CSV / DSV")
	add_child(fd)
	fd.file_selected.connect(func(p: String):
		set_source_file(p)
		fd.queue_free())
	fd.canceled.connect(func(): fd.queue_free())
	fd.popup_centered_ratio(0.5)


func _on_export_csv() -> void:
	if not _editing_allowed or _source_path.is_empty():
		_set_status("nothing to export")
		return
	var fd := FileDialog.new()
	fd.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	fd.access = FileDialog.ACCESS_FILESYSTEM
	fd.exclusive = false
	fd.add_filter("*.csv", "CSV")
	fd.current_file = _source_path.get_file()
	add_child(fd)
	fd.file_selected.connect(func(p: String):
		_export_csv_to(p)
		fd.queue_free())
	fd.canceled.connect(func(): fd.queue_free())
	fd.popup_centered_ratio(0.5)


func _export_csv_to(path: String) -> void:
	var all: Array = []
	all.append(_headers)
	for row in _rows:
		all.append(row)
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var err := w.write_rows(all, path, PackedStringArray())
	if err == OK:
		_set_status("exported CSV -> " + path)
		print("VortarisCSV editor: exported ", path)
	else:
		_set_status("export failed: %s" % error_string(err))


func _on_export_rows() -> void:
	if not _editing_allowed or _source_path.is_empty():
		_set_status("nothing to export")
		return
	var selected: Array = []
	var item := _tree.get_next_selected(null)
	while item != null:
		var data_row := int(item.get_meta("data_row", -1))
		if data_row >= 0 and data_row < _rows.size():
			selected.append(_rows[data_row])
		item = _tree.get_next_selected(item)
	if selected.is_empty():
		_set_status("select one or more rows to export")
		return
	var fd := FileDialog.new()
	fd.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	fd.access = FileDialog.ACCESS_FILESYSTEM
	fd.exclusive = false
	fd.add_filter("*.csv", "CSV")
	fd.current_file = _source_path.get_basename() + "_rows.csv"
	add_child(fd)
	fd.file_selected.connect(func(p: String):
		var w := VCSVWriter.new()
		w.line_ending = "\n"
		var err := w.write_rows(selected, p, _headers)
		if err == OK:
			_set_status("exported %d row(s) -> %s" % [selected.size(), p])
			print("VortarisCSV editor: exported rows ", selected.size(), " -> ", p)
		else:
			_set_status("export failed: %s" % error_string(err))
		fd.queue_free())
	fd.canceled.connect(func(): fd.queue_free())
	fd.popup_centered_ratio(0.5)


# ---------------------------------------------------------------------------
# UI helpers
# ---------------------------------------------------------------------------

func _add_btn(parent: Control, text: String, callable: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.pressed.connect(callable)
	parent.add_child(b)
	return b


func _section_title(text: String) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", 16)
	l.add_theme_color_override("font_color", Color(0.8, 0.8, 0.85))
	return l


func _set_status(text: String) -> void:
	if _status != null:
		_status.text = text
