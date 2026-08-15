@tool
extends PanelContainer

# Editor-only preview dock: renders the raw CSV grid of the currently selected
# .csv file and allows double-click cell editing (writes back to the source .csv
# and reimports). Wired up by demo/addons/vortariscsv/editor_plugin.gd.

const MAX_ROWS := 200

var _tree: Tree
var _path_label: Label
var _source_path := ""
var _headers: PackedStringArray = PackedStringArray()
var _rows: Array = []
var _editing_allowed := false

# Re-entrancy guard for the deferred reimport (see _queue_reimport/_do_reimport).
var _reimport_pending := false
# Path captured at queue time. _do_reimport runs on the next frame, by which time
# _source_path may already point at a different (newly selected) file — always
# reimport the file that was actually edited, not whatever is selected now.
var _reimport_path := ""


func _ready() -> void:
	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(vb)

	_path_label = Label.new()
	_path_label.text = "No .csv selected"
	vb.add_child(_path_label)

	_tree = Tree.new()
	_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tree.columns = 1
	_tree.custom_minimum_size = Vector2(0, 220)
	_tree.item_edited.connect(_on_item_edited)
	vb.add_child(_tree)


# Called by the plugin when a .csv is selected (or deselected with "").
func set_source_file(path: String) -> void:
	_source_path = path
	if path.is_empty() or not path.ends_with(".csv"):
		_path_label.text = "No .csv selected"
		_tree.clear()
		_editing_allowed = false
		return
	var r := VCSVParser.parse_file(path, null)
	_path_label.text = path
	if r == null or not r.success:
		_tree.clear()
		_editing_allowed = false
		return
	_headers = r.table.headers
	_rows = r.table.rows
	_editing_allowed = true
	_populate()


func _populate() -> void:
	_tree.clear()
	_tree.columns = maxi(1, _headers.size())
	for c in _headers.size():
		_tree.set_column_title(c, _headers[c])
	var root := _tree.create_item()
	root.set_text(0, " ")
	for i in mini(_rows.size(), MAX_ROWS):
		var item := _tree.create_item()
		var row: PackedStringArray = _rows[i]
		for c in _headers.size():
			item.set_text(c, row[c] if c < row.size() else "")
			item.set_editable(c, true)
	if _rows.size() > MAX_ROWS:
		var note := _tree.create_item()
		note.set_text(0, "... %d more rows (preview capped)" % (_rows.size() - MAX_ROWS))


func _on_item_edited() -> void:
	if not _editing_allowed or _source_path.is_empty():
		return
	var item := _tree.get_edited()
	if item == null:
		return
	var col := _tree.get_edited_column()
	var data_row := item.get_index() - 1 # data rows start after the root item
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
		printerr("VortarisCSV preview: failed to write ", _source_path, " (err ", err, ")")
		return
	# Reimport so the imported .tres picks up the change. reimport_files() shows
	# the editor progress dialog; calling it synchronously from an item_edited
	# handler (message-queue flush context) prints a stack of
	# editor/gui/progress_dialog.cpp errors ("Do not use progress dialog while
	# flushing the message queue or using call_deferred()!"). Defer to the next
	# process frame (one-shot, with a re-entrancy guard).
	_queue_reimport()
	print("VortarisCSV preview: wrote ", _source_path)


func _queue_reimport() -> void:
	if _reimport_pending:
		return
	_reimport_pending = true
	# Capture the path now: _do_reimport runs on the next idle frame, by which
	# time _source_path may have changed (a different file selected meanwhile).
	_reimport_path = _source_path
	# process_frame fires at the start of the next idle frame, outside the
	# message-queue flush. (call_deferred on reimport_files itself still runs
	# inside MessageQueue::flush() and re-triggers the error.)
	var tree := get_tree()
	if tree == null:
		# Panel not in a scene tree (shouldn't happen while editing), nothing to
		# reimport against — drop the pending flag and return.
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
	var plugin: EditorPlugin = get_meta("plugin_ref", null)
	if plugin == null:
		return
	var fs := plugin.get_editor_interface().get_resource_filesystem()
	if fs == null:
		return
	fs.reimport_files(PackedStringArray([path]))
