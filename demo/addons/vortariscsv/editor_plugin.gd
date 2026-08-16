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
	# 注册/迁移所有 vortariscsv/* 项目设置（general / import / editor / validation）。
	_register_project_settings()

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


# ---------------------------------------------------------------------------
# Project settings (v0.3.x hierarchical layout)
# ---------------------------------------------------------------------------

## 注册/确保所有 vortariscsv/* 项目设置，并迁移 0.2.x 的旧扁平路径。
## 仅在 !has_setting 时写值，避免覆盖用户已显式配置的设置（参照 ML F4 修复）。
##
## 设置定义表：每行 [path, default, type, hint, hint_string, old_path, description]。
##   - hint_string 只用于它的语义用途（枚举选项 / 数值范围 / 占位文本）；绝不塞入
##     自由文本说明 —— 对 enum/range/array 等类型，Godot 会解析 hint_string，塞说明
##     会破坏解析（参照 ModLoader "Cannot get class" 教训）。
##   - description 仅作为代码内文档与未来兼容：Godot 4.7 的 ProjectSettings 面板
##     不渲染自定义设置的 tooltip/description（内置设置说明是引擎编译时硬编码的），
##     所以真正的说明写在 README 的设置参考表里。
##   - 唯一的兼容回退是 C++ 侧 get_setting_with_fallback()（src/core/vcsv_settings.h），
##     启动时旧路径值会迁到新路径并 erase 掉，确保 Project Settings 只显示一个路径。
const _SETTING_DEFS := [
	["vortariscsv/general/verbose", false, TYPE_BOOL, PROPERTY_HINT_NONE, "", "vortariscsv/verbose",
		"Print [vortariscsv][v] verbose log lines (debug builds only)."],
	["vortariscsv/general/lazy_build_default", false, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Default for new tables' lazy_build (VCSVDataTable.from_file / editor import)."],
	["vortariscsv/general/hot_reload_default", false, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Default for new tables' hot_reload (re-import the .tres when the .csv changes)."],
	["vortariscsv/import/override_translation_importer", true, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Let the Vortaris importer take over .csv/.tsv by default (import priority 2.0)."],
	["vortariscsv/import/delimiter", ",", TYPE_STRING, PROPERTY_HINT_PLACEHOLDER_TEXT,
		"default delimiter for imported CSVs (e.g. , ; tab |)", "",
		"Default delimiter used by the editor import."],
	["vortariscsv/import/encoding", "utf8", TYPE_STRING, PROPERTY_HINT_ENUM, "utf8,gbk,gb2312", "",
		"Default text encoding used by the editor import."],
	["vortariscsv/import/auto_detect_delimiter", false, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Auto-detect the delimiter on import instead of using the delimiter default."],
	["vortariscsv/import/header_rows", 1, TYPE_INT, PROPERTY_HINT_RANGE, "1,10,1", "",
		"Number of leading header rows in imported CSVs."],
	["vortariscsv/editor/table_font_size", 14, TYPE_INT, PROPERTY_HINT_RANGE, "8,32,1", "",
		"Font size of the CSV main-screen data table."],
	["vortariscsv/editor/auto_switch_to_csv", true, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Double-clicking a Vortaris-imported .csv in the FileSystem dock switches to the CSV main screen."],
	["vortariscsv/validation/check_duplicate_keys", true, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Default: VCSVDataTable.validate() reports duplicate key-column values."],
	["vortariscsv/validation/check_required_columns", true, TYPE_BOOL, PROPERTY_HINT_NONE, "", "",
		"Default: VCSVDataTable.validate() reports missing required_columns."],
]

func _register_project_settings() -> void:
	var saved := false
	for def in _SETTING_DEFS:
		if _ensure_setting(def[0], def[1], def[2], def[3], def[4], def[5], def[6]):
			saved = true
	if saved:
		ProjectSettings.save()


## 确保一项设置存在，并注册其属性信息（让它在 Project Settings 面板里以正确的
## 类型/hint 显示）。仅在设置缺失时写入值：有 old_path 且旧路径存在时迁移旧值，
## 否则写默认值。已存在的设置绝不覆盖。
## 迁移/清理后会把旧扁平路径 erase 掉，确保 Project Settings 面板只显示新路径；
## 旧路径仅作为 C++ 侧 get_setting_with_fallback() 的兼容读取回退，不再作为可见设置。
## 返回 true 表示本次写/清了值（调用方应 save）。
func _ensure_setting(path: String, default_value: Variant, type: int,
		property_hint: int = PROPERTY_HINT_NONE, hint_string: String = "",
		old_path: String = "", description: String = "") -> bool:
	var changed := false
	if not ProjectSettings.has_setting(path):
		if not old_path.is_empty() and ProjectSettings.has_setting(old_path):
			ProjectSettings.set_setting(path, ProjectSettings.get_setting(old_path))
		else:
			ProjectSettings.set_setting(path, default_value)
		changed = true
	# 清理旧扁平路径：迁移后 erase，避免它继续以可见设置的形式出现在 Project Settings。
	# 即使新路径已存在（用户已显式配置），残留的旧路径也一并清掉（读取走新路径）。
	if not old_path.is_empty() and ProjectSettings.has_setting(old_path):
		ProjectSettings.clear(old_path)
		changed = true
	var info := {"name": path, "type": type}
	if property_hint != PROPERTY_HINT_NONE:
		info["property_hint"] = property_hint
	if not hint_string.is_empty():
		info["hint_string"] = hint_string
	if not description.is_empty():
		# Godot 4.7 不渲染该键（见 _SETTING_DEFS 注释），仅作代码内文档与未来兼容。
		info["description"] = description
	ProjectSettings.add_property_info(info)
	return changed


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
	# A .csv (Vortaris-imported) became the FileSystem selection. A single click
	# (selection) must NOT yank the user out of their current editor — only a
	# real double-click (detected in _on_fs_selection_changed via the 500 ms
	# re-click window) opens it in the CSV tab. Here we just open the file when
	# the CSV tab is already active, otherwise remember it as a pending file.
	_open_csv_in_editor(sel, false)


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
	# vortariscsv/editor/auto_switch_to_csv (default true): double-clicking a
	# Vortaris .csv switches to the CSV tab. When disabled, the file is still
	# opened whenever the CSV tab is already visible, otherwise kept as a pending
	# file — the user's current editor is never yanked away.
	var want_switch := switch_tab and bool(ProjectSettings.get_setting(
			"vortariscsv/editor/auto_switch_to_csv", true))
	if want_switch or _main_screen.visible:
		if want_switch:
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
