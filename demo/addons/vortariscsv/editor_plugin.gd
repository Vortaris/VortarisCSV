@tool
extends EditorPlugin

# VortarisCSV 的运行时由 vortariscsv.gdextension 加载，与编辑器插件面板无关。
# 此 EditorPlugin 负责：
#   1. 让插件出现在 Project > Plugins 中，便于启用/管理；
#   2. 注册 C++ 导入插件 VCSVEditorImportPlugin（.csv/.tsv -> .tres 数据表）；
#   3. 主动确保 vortariscsv/import/override_translation_importer=true（未设置时），
#      使 VortarisCSV 默认接管 .csv 导入，而不是 Godot 内置翻译 CSV 导入器；
#   4. 提供 Tool 菜单项，一键把存量 .csv 从翻译导入器切换到 VortarisCSV。
#
# The VortarisCSV runtime is loaded from vortariscsv.gdextension automatically.
# This EditorPlugin exists so the add-on shows up in Project > Plugins and can
# register the C++ EditorImportPlugin that imports .csv/.tsv as data tables.

var _import_plugin = null
var _preview: Control = null
var _last_preview_path := ""

# Tool menu entry label (also used to remove it in _exit_tree).
const TOOL_MENU_SWITCH := "VortarisCSV: .csv -> Vortaris importer"
# Dock label (also used to remove it in _exit_tree).
const PREVIEW_DOCK_NAME := "VortarisCSV"


func _enter_tree() -> void:
	# 默认接管 .csv 导入：未显式设置该开关时写入 true，避免用户静默落到
	# Godot 内置翻译 CSV 导入器。已显式设为 false 的用户不会被覆盖。
	if not ProjectSettings.has_setting("vortariscsv/import/override_translation_importer"):
		ProjectSettings.set_setting("vortariscsv/import/override_translation_importer", true)
		ProjectSettings.save()

	# 仅在 C++ 侧已注册 VCSVEditorImportPlugin 时挂载导入插件。
	if not ClassDB.class_exists("VCSVEditorImportPlugin"):
		return
	_import_plugin = VCSVEditorImportPlugin.new()
	add_import_plugin(_import_plugin)
	add_tool_menu_item(TOOL_MENU_SWITCH, Callable(self, "_convert_csvs_to_vortariscsv"))

	# 热重载：文件系统扫描结束后，对已注册的热重载表调用 poll_hot_reload()。
	if ClassDB.class_exists("VCSVDataTable"):
		var fs := get_editor_interface().get_resource_filesystem()
		if fs != null:
			fs.filesystem_changed.connect(_on_filesystem_changed)

	# 表格预览面板：选中 .csv 时渲染原始网格，双击单元格编辑并回写。
	# 文件选择变化通过 _process 轮询 EditorInterface.get_selected_paths() 检测
	# （FileSystemDock 的 file_selected 信号在 GDScript 侧不可靠）。
	if ClassDB.class_exists("VCSVParser") and ClassDB.class_exists("VCSVWriter"):
		_preview = preload("res://addons/vortariscsv/editor_table_preview.gd").new()
		_preview.set_meta("plugin_ref", self)
		_preview.name = PREVIEW_DOCK_NAME
		add_control_to_dock(DOCK_SLOT_RIGHT_BL, _preview)
		set_process(true)


func _process(_delta: float) -> void:
	if _preview == null:
		return
	var sel := ""
	for p in get_editor_interface().get_selected_paths():
		if p.ends_with(".csv"):
			sel = p
			break
	if sel != _last_preview_path:
		_last_preview_path = sel
		_preview.set_source_file(sel)


func _exit_tree() -> void:
	if _import_plugin != null:
		remove_import_plugin(_import_plugin)
		_import_plugin = null
	remove_tool_menu_item(TOOL_MENU_SWITCH)
	if _preview != null:
		remove_control_from_docks(_preview)
		_preview.queue_free()
		_preview = null
	if ClassDB.class_exists("VCSVDataTable"):
		var fs := get_editor_interface().get_resource_filesystem()
		if fs != null and fs.filesystem_changed.is_connected(_on_filesystem_changed):
			fs.filesystem_changed.disconnect(_on_filesystem_changed)


func _on_filesystem_changed() -> void:
	if not ClassDB.class_exists("VCSVDataTable"):
		return
	for tbl in VCSVDataTable.get_hot_tables():
		if is_instance_valid(tbl):
			tbl.poll_hot_reload()


# Tool menu handler: switches every .csv currently imported by Godot's built-in
# translation importer over to the VortarisCSV importer, then reimports them.
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
