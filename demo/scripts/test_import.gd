extends SceneTree

# Verifies the import pipeline logic used by VCSVEditorImportPlugin._import:
# parse -> build VCSVDataTable -> save .tres -> reload and bind.
# (The EditorImportPlugin glue itself is exercised by running the editor.)
# Run: Godot --headless --path demo --script res://scripts/test_import.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func test_runtime_import_pipeline() -> void:
	var r := VCSVParser.parse_file("res://data/monsters.csv", null)
	check(r.success, "parse monsters.csv")
	check(r.table != null, "table parsed")

	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = t.headers[0]
	t.row_type = "res://scripts/row_types/monster_row.gd"
	# Mimic the importer's type inference (via the same public pieces).
	var types := VCSVUtil.detect_types(t.to_table(), ";", true)
	check(types["health"] == "int", "import infers int")
	check(types["attack"] == "float", "import infers float")
	check(types["alive"] == "bool", "import infers bool")

	var path := "user://test_import_monsters.tres"
	var err := ResourceSaver.save(t, path)
	check(err == OK, "saved .tres")

	var loaded = load(path) as VCSVDataTable
	check(loaded != null, "reloaded .tres is VCSVDataTable")
	check(loaded.get_row_type() == "res://scripts/row_types/monster_row.gd", "row_type persisted")
	check(loaded.get_headers() == r.table.headers, "headers persisted")
	check(loaded.row_count() == 2, "rows persisted")

	var goblin: MonsterRow = loaded.get_row("goblin")
	check(goblin != null, "row built from reloaded .tres")
	check(goblin.health == 100, "health after reload")
	check(goblin.name == "哥布林", "name after reload")
	check(goblin.position == Vector2(10, 20), "Vector2 after reload")
	check(goblin.color == Color("#ff0000"), "Color after reload")
	check(goblin.tags == [1, 2, 3], "typed array after reload")


func test_editor_import() -> void:
	# Only meaningful after the plugin is enabled in Project > Plugins and the
	# editor has re-imported data/monsters.csv with our importer (priority 2.0
	# replaces the built-in translation importer). Skips otherwise.
	var import_path := "res://data/monsters.csv.import"
	if not FileAccess.file_exists(import_path):
		print("SKIP test_editor_import: enable the VortarisCSV plugin in Project > Plugins, then reopen the editor")
		return
	var f := FileAccess.open(import_path, FileAccess.READ)
	var meta := f.get_as_text()
	f.close()
	if not meta.contains('importer="vortariscsv"'):
		print("SKIP test_editor_import: monsters.csv is imported by the built-in translation importer; enable VortarisCSV and reimport")
		return
	var res = load("res://data/monsters.csv")
	check(res is VCSVDataTable, "editor-imported csv loads as VCSVDataTable")
	if res is VCSVDataTable:
		var table := res as VCSVDataTable
		check(table.row_count() == 2, "editor-imported row count")
		var goblin: MonsterRow = table.get_row("goblin")
		check(goblin != null and goblin.health == 100, "editor-imported binding works")


func _init() -> void:
	test_runtime_import_pipeline()
	test_editor_import()
	if failures == 0:
		print("test_import OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_import FAILED: ", failures, "/", checks, " failed")
		quit(1)
