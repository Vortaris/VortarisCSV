@tool
extends EditorPlugin
## VortarisCSV editor plugin.
##
## VortarisCSV now lives in the **main screen** (the "CSV" tab, next to
## 2D/3D/Script/AssetLib), not the right dock. `csv_main_screen.gd` hosts the
## editable, resizable data table + details/validation panel. The old right-dock
## preview (`editor_table_preview.gd`) and its *Tools* toggle were removed — the
## main screen supersedes them.
##
## The plugin also:
##   1. Registers the C++ import plugin VCSVEditorImportPlugin (.csv/.tsv ->
##      .tres data table).
##   2. Ensures `vortariscsv/import/override_translation_importer=true` (the
##      Vortaris importer takes over .csv by default) and provides a *Tools* item
##      that flips any remaining translation-imported .csv over.
##   3. Polls the FileSystem dock selection: activating a .csv imported by
##      VortarisCSV switches to the CSV tab and opens it in the editor (double-
##      clicking a .csv "opens" it exactly like any other editor asset).

var _import_plugin = null
var _main_screen: Control = null

# Double-click / activation tracking for FileSystem-dock .csv files.
const DOUBLE_CLICK_MS := 500
var _last_selected_csv := ""
var _last_click_path := ""
var _last_click_msec := 0

# Tool menu entry label (also used to remove it in _exit_tree).
const TOOL_MENU_SWITCH := "VortarisCSV: .csv -> Vortaris importer"
const MAIN_SCREEN_NAME := "CSV"


func _enter_tree() -> void:
	# 默认接管 .csv 导入：未显式设置该开关时写入 true，避免用户静默落到
	# Godot 内置翻译 CSV 导入器。已显式设为 false 的用户不会被覆盖。
	if not ProjectSettings.has_setting("vortariscsv/import/override_translation_importer"):
		ProjectSettings.set_setting("vortariscsv/import/override_translation_importer", true)
		ProjectSettings.save()

	# 注册 vortariscsv/verbose 日志开关（默认 false）。
	if not ProjectSettings.has_setting("vortariscsv/verbose"):
		ProjectSettings.set_setting("vortariscsv/verbose", false)
		ProjectSettings.save()
	ProjectSettings.add_property_info({
		"name": "vortariscsv/verbose",
		"type": TYPE_BOOL,
		"hint_string": "Print [vortariscsv][v] verbose log lines (debug builds only).",
	})

	# 仅在 C++ 侧已注册 VCSVEditorImportPlugin 时挂载导入插件。
	if ClassDB.class_exists("VCSVEditorImportPlugin"):
		_import_plugin = VCSVEditorImportPlugin.new()
		add_import_plugin(_import_plugin)
		add_tool_menu_item(TOOL_MENU_SWITCH, Callable(self, "_convert_csvs_to_vortariscsv"))

	# 热重载：文件系统扫描结束后，对已注册的热重载表调用 poll_hot_reload()。
	if ClassDB.class_exists("VCSVDataTable"):
		var fs := get_editor_interface().get_resource_filesystem()
		if fs != null:
			fs.filesystem_changed.connect(_on_filesystem_changed)

	# 主窗口（CSV tab）：表格编辑界面，替换旧的右 dock 预览。
	if ClassDB.class_exists("VCSVParser") and ClassDB.class_exists("VCSVWriter"):
		_main_screen = preload("res://addons/vortariscsv/csv_main_screen.gd").new()
		_main_screen.set_meta("plugin_ref", self)
		_main_screen.name = "VortarisCSV"
		EditorInterface.get_editor_main_screen().add_child(_main_screen)
		_make_visible(false)
		# FileSystemDock 的选择信号（editor-only；运行时不存在该控件）。
		var fs_dock = EditorInterface.get_file_system_dock()
		if fs_dock != null and not fs_dock.selection_changed.is_connected(_on_fs_selection_changed):
			fs_dock.selection_changed.connect(_on_fs_selection_changed)
		set_process(true)
		# 初始化跟踪的选中路径：编辑器启动时若已选中某个 .csv，把它记为
		# "已知选中"，避免第一次 _process 误判为新激活而强制切到 CSV tab。
		_last_selected_csv = selected_csv_path()
		if not _last_selected_csv.is_empty():
			_main_screen.set_pending_file(_last_selected_csv)


func _exit_tree() -> void:
	if _import_plugin != null:
		remove_import_plugin(_import_plugin)
		_import_plugin = null
	remove_tool_menu_item(TOOL_MENU_SWITCH)
	if _main_screen != null:
		if _main_screen.is_inside_tree():
			EditorInterface.get_editor_main_screen().remove_child(_main_screen)
		_main_screen.queue_free()
		_main_screen = null
	var fs_dock = EditorInterface.get_file_system_dock()
	if fs_dock != null and fs_dock.selection_changed.is_connected(_on_fs_selection_changed):
		fs_dock.selection_changed.disconnect(_on_fs_selection_changed)
	if ClassDB.class_exists("VCSVDataTable"):
		var fs := get_editor_interface().get_resource_filesystem()
		if fs != null and fs.filesystem_changed.is_connected(_on_filesystem_changed):
			fs.filesystem_changed.disconnect(_on_filesystem_changed)


# ---------------------------------------------------------------------------
# Main screen (V1)
# ---------------------------------------------------------------------------

func _has_main_screen() -> bool:
	return true


func _make_visible(visible: bool) -> void:
	if _main_screen == null:
		return
	_main_screen.visible = visible
	if visible:
		_main_screen.on_plugin_tab_shown()


func _get_plugin_name() -> String:
	return MAIN_SCREEN_NAME


func _get_plugin_icon() -> Texture2D:
	# Small main-screen tab icon. Godot renders the icon at native texture size
	# in the main-screen tab button, so the 64x64 icon.svg would blow the tab up.
	# ResourceLoader.exists() avoids the "Failed loading resource" ERROR on the
	# very first editor run, before the SVG import cache is built.
	var icon_path := "res://addons/vortariscsv/icon_main.svg"
	if ResourceLoader.exists(icon_path):
		var icon = load(icon_path)
		if icon is Texture2D:
			return icon
	return EditorInterface.get_editor_theme().get_icon("Node", "EditorIcons")


# ---------------------------------------------------------------------------
# FileSystem-dock activation (V2)
# ---------------------------------------------------------------------------

func _process(_delta: float) -> void:
	if _main_screen == null:
		return
	# Cheap pass every frame: just look for a .csv in the selection (no .import
	# reads). Only when the selected .csv path changes do we do the heavier
	# importer-ownership check.
	var sel := ""
	for p in get_editor_interface().get_selected_paths():
		if p.ends_with(".csv"):
			sel = p
			break
	if sel == _last_selected_csv:
		return
	_last_selected_csv = sel
	if sel.is_empty():
		return
	if not _is_vortariscsv_csv(sel):
		# A .csv we don't own (Godot's translation importer): keep default editor.
		return
	# A .csv (Vortaris-imported) became the FileSystem selection. The first click
	# of a double-click is indistinguishable from a single click here, so we open
	# it in the CSV editor; if the tab isn't active yet we switch to it, which is
	# exactly the "double-click a .csv opens its editor" behaviour.
	_last_click_path = sel
	_last_click_msec = Time.get_ticks_msec()
	_open_csv_in_editor(sel, true)


func _on_fs_selection_changed() -> void:
	# Signal-driven selection tracking. When the same .csv is re-reported inside
	# the double-click window (a real double-click on an already-selected file),
	# switch to the CSV tab too.
	var sel := selected_csv_path()
	if sel.is_empty():
		return
	var now := Time.get_ticks_msec()
	if sel == _last_click_path and now - _last_click_msec < DOUBLE_CLICK_MS:
		_open_csv_in_editor(sel, true)
		_last_click_path = ""
		_last_click_msec = 0
	else:
		_last_click_path = sel
		_last_click_msec = now


func _open_csv_in_editor(path: String, switch_tab: bool) -> void:
	if _main_screen == null:
		return
	if switch_tab or _main_screen.visible:
		if switch_tab:
			EditorInterface.set_main_screen_editor(MAIN_SCREEN_NAME)
		_main_screen.set_source_file(path)
	else:
		_main_screen.set_pending_file(path)


## Returns the first FileSystem-dock selected .csv that the Vortaris importer
## owns (or a raw .csv with no import metadata yet), else "".
func selected_csv_path() -> String:
	for p in get_editor_interface().get_selected_paths():
		if p.ends_with(".csv") and _is_vortariscsv_csv(p):
			return p
	return ""


## A .csv is "ours" when its .import says importer=vortariscsv, or when there is
## no .import yet (brand-new file). Translation-imported .csv stays on Godot's
## default editor and is ignored here.
func _is_vortariscsv_csv(path: String) -> bool:
	var import_path := path + ".import"
	if not FileAccess.file_exists(import_path):
		return true
	var cf := ConfigFile.new()
	if cf.load(import_path) != OK:
		return false
	return cf.get_value("remap", "importer", "") == "vortariscsv"


func _on_filesystem_changed() -> void:
	if not ClassDB.class_exists("VCSVDataTable"):
		return
	for tbl in VCSVDataTable.get_hot_tables():
		if is_instance_valid(tbl):
			tbl.poll_hot_reload()


# ---------------------------------------------------------------------------
# Tool menu: switch .csv importer
# ---------------------------------------------------------------------------

func _convert_csvs_to_vortariscsv(_userdata = null) -> void:
	var csv_files: PackedStringArray = []
	_collect_csv("res://", csv_files)
	if csv_files.is_empty():
		printerr("VortarisCSV: no .csv files found under res://")
		return

	var changed: PackedStringArray = []
	for path in csv_files:
		var import_path := path + ".import"
		if not FileAccess.file_exists(import_path):
			continue
		var cf := ConfigFile.new()
		if cf.load(import_path) != OK:
			continue
		if cf.get_value("remap", "importer", "") == "vortariscsv":
			continue
		cf.set_value("remap", "importer", "vortariscsv")
		cf.set_value("remap", "type", "VCSVDataTable")
		# Drop the old importer's params; the new importer regenerates defaults.
		if cf.has_section("params"):
			cf.erase_section("params")
		if cf.save(import_path) == OK:
			changed.append(path)

	if changed.is_empty():
		print("VortarisCSV: all .csv files already use the Vortaris importer")
		return
	get_editor_interface().get_resource_filesystem().reimport_files(changed)
	print("VortarisCSV: switched ", changed.size(), " .csv file(s) to the Vortaris importer and queued reimport")


func _collect_csv(dir_path: String, out: PackedStringArray) -> void:
	var da := DirAccess.open(dir_path)
	if da == null:
		return
	da.list_dir_begin()
	var entry := da.get_next()
	while entry != "":
		if da.current_is_dir():
			if not entry.begins_with(".") and entry != ".godot" and entry != ".import":
				_collect_csv(dir_path.path_join(entry), out)
		elif entry.ends_with(".csv"):
			out.append(dir_path.path_join(entry))
		entry = da.get_next()
	da.list_dir_end()
