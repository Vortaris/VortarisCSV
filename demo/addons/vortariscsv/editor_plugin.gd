@tool
extends EditorPlugin

# VortarisCSV 的运行时由 vortariscsv.gdextension 加载，与编辑器插件面板无关。
# 此 EditorPlugin 负责：
#   1. 让插件出现在 Project > Plugins 中，便于启用/管理；
#   2. 注册 C++ 导入插件 VCSVEditorImportPlugin（.csv/.tsv -> .tres 数据表）。
#
# The VortarisCSV runtime is loaded from vortariscsv.gdextension automatically.
# This EditorPlugin exists so the add-on shows up in Project > Plugins and can
# register the C++ EditorImportPlugin that imports .csv/.tsv as data tables.

var _import_plugin = null


func _enter_tree() -> void:
	# 仅在 C++ 侧已注册 VCSVEditorImportPlugin 时挂载导入插件。
	if not ClassDB.class_exists("VCSVEditorImportPlugin"):
		return
	# 覆盖开关：vortariscsv/import/override_translation_importer（默认 true）。
	# true  -> 导入插件以高优先级覆盖 Godot 内置翻译 CSV 导入器；
	# false -> 插件仍可用，但 .csv 默认走内置翻译导入器（可在导入面板按资产切换）。
	_import_plugin = VCSVEditorImportPlugin.new()
	add_import_plugin(_import_plugin)


func _exit_tree() -> void:
	if _import_plugin != null:
		remove_import_plugin(_import_plugin)
		_import_plugin = null
