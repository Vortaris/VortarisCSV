extends SceneTree

# Regression tests for the hierarchical project-settings layout (v0.3.x):
#   - new-path default value reading
#   - VCSVDataTable.from_file applies vortariscsv/general/{lazy_build_default,hot_reload_default}
#   - validate() applies vortariscsv/validation/* defaults (explicit option wins)
# Run: Godot --headless --path demo --script res://scripts/test_settings.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func has_issue(issues: PackedStringArray, needle: String) -> bool:
	for issue in issues:
		if needle in issue:
			return true
	return false


func test_default_reads() -> void:
	# The editor plugin does not run in headless --script mode, so the settings
	# are not registered via add_property_info; the documented defaults must hold
	# through the fallback default argument.
	check(ProjectSettings.get_setting("vortariscsv/general/verbose", false) == false,
			"general/verbose default false")
	check(ProjectSettings.get_setting("vortariscsv/general/lazy_build_default", false) == false,
			"general/lazy_build_default default false")
	check(ProjectSettings.get_setting("vortariscsv/general/hot_reload_default", false) == false,
			"general/hot_reload_default default false")
	check(ProjectSettings.get_setting("vortariscsv/import/override_translation_importer", true) == true,
			"import/override_translation_importer default true")
	check(ProjectSettings.get_setting("vortariscsv/import/delimiter", ",") == ",",
			"import/delimiter default ','")
	check(ProjectSettings.get_setting("vortariscsv/import/encoding", "utf8") == "utf8",
			"import/encoding default utf8")
	check(ProjectSettings.get_setting("vortariscsv/import/auto_detect_delimiter", false) == false,
			"import/auto_detect_delimiter default false")
	check(ProjectSettings.get_setting("vortariscsv/import/header_rows", 1) == 1,
			"import/header_rows default 1")
	check(ProjectSettings.get_setting("vortariscsv/editor/table_font_size", 14) == 14,
			"editor/table_font_size default 14")
	check(ProjectSettings.get_setting("vortariscsv/validation/check_duplicate_keys", true) == true,
			"validation/check_duplicate_keys default true")
	check(ProjectSettings.get_setting("vortariscsv/validation/check_required_columns", true) == true,
			"validation/check_required_columns default true")


func test_from_file_defaults() -> void:
	ProjectSettings.set_setting("vortariscsv/general/lazy_build_default", true)
	ProjectSettings.set_setting("vortariscsv/general/hot_reload_default", true)
	var t := VCSVDataTable.from_file("res://data/monsters.csv", null,
			"res://scripts/row_types/monster_row.gd")
	check(t != null, "from_file ok")
	if t:
		check(t.lazy_build == true, "from_file applies lazy_build_default=true")
		check(t.hot_reload == true, "from_file applies hot_reload_default=true")
		# Explicit per-table override still wins after creation.
		t.lazy_build = false
		t.hot_reload = false
		check(t.lazy_build == false and t.hot_reload == false, "explicit override wins")
	# Reset so later checks start from the documented default.
	ProjectSettings.set_setting("vortariscsv/general/lazy_build_default", false)
	ProjectSettings.set_setting("vortariscsv/general/hot_reload_default", false)


func test_validation_defaults() -> void:
	# check_duplicate_keys
	var r := VCSVParser.parse_string("id,name\nk1,A\nk1,B\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"

	ProjectSettings.set_setting("vortariscsv/validation/check_duplicate_keys", false)
	check(not has_issue(t.validate(), "duplicate key"),
			"check_duplicate_keys=false skips duplicate-key check")

	ProjectSettings.set_setting("vortariscsv/validation/check_duplicate_keys", true)
	check(has_issue(t.validate(), "duplicate key"),
			"check_duplicate_keys=true reports duplicate key")

	# An explicit option overrides the project setting.
	ProjectSettings.set_setting("vortariscsv/validation/check_duplicate_keys", false)
	check(has_issue(t.validate({"check_duplicate_keys": true}), "duplicate key"),
			"explicit check_duplicate_keys=true wins over setting")

	# check_required_columns
	var r2 := VCSVParser.parse_string("id,name\nk1,A\n", null)
	var t2 := VCSVDataTable.new()
	t2.headers = r2.table.headers
	t2.rows = r2.table.rows
	t2.key_column = "id"

	ProjectSettings.set_setting("vortariscsv/validation/check_required_columns", false)
	check(not has_issue(t2.validate({"required_columns": ["id", "hp"]}), "missing required column 'hp'"),
			"check_required_columns=false skips required-column check")

	ProjectSettings.set_setting("vortariscsv/validation/check_required_columns", true)
	check(has_issue(t2.validate({"required_columns": ["id", "hp"]}), "missing required column 'hp'"),
			"check_required_columns=true reports missing required column")

	# Reset.
	ProjectSettings.set_setting("vortariscsv/validation/check_duplicate_keys", true)
	ProjectSettings.set_setting("vortariscsv/validation/check_required_columns", true)


func _init() -> void:
	test_default_reads()
	test_from_file_defaults()
	test_validation_defaults()
	if failures == 0:
		print("test_settings OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_settings FAILED: ", failures, "/", checks, " failed")
		quit(1)
