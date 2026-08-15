extends SceneTree

# Data-integrity validation tests for VCSVDataTable.validate() plus the
# first-wins duplicate-key behaviour of ensure_index().
# Run: Godot --headless --path demo --script res://scripts/test_validation.gd

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


func test_required_columns() -> void:
	var r := VCSVParser.parse_string("id,name\nk1,A\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	var issues := t.validate({"required_columns": ["id", "hp"]})
	check(has_issue(issues, "missing required column 'hp'"), "validate reports missing required column")
	check(not has_issue(issues, "missing required column 'id'"), "present column not reported")


func test_conversion_errors() -> void:
	var r := VCSVParser.parse_string("id,health\nk1,notanumber\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/monster_row.gd"
	var issues := t.validate()
	check(has_issue(issues, "row:1"), "validate surfaces type-conversion failure with row tag")


func test_duplicate_ids() -> void:
	var r := VCSVParser.parse_string("id,name\nk1,A\nk1,B\nk2,C\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	var issues := t.validate()
	check(has_issue(issues, "duplicate key 'k1'"), "validate reports duplicate id")

	# ensure_index keeps the first occurrence and records a warning.
	check(t.get_row_dict("k1")["name"] == "A", "duplicate keeps first occurrence")
	var warned := false
	for w in t.get_last_warnings():
		if "duplicate key" in w and "k1" in w:
			warned = true
	check(warned, "ensure_index records duplicate-key warning")


func test_fk_missing() -> void:
	var msrc := "id,name\nh1,goblin\n"
	var mr := VCSVParser.parse_string(msrc, null)
	var monsters := VCSVDataTable.new()
	monsters.headers = mr.table.headers
	monsters.rows = mr.table.rows
	monsters.key_column = "id"
	monsters.row_type = "res://scripts/row_types/monster_row.gd"
	var mpath := "user://test_validation_monsters.tres"
	ResourceSaver.save(monsters, mpath)

	var wsrc := "id,owner\nw1,h1\nw2,ghost\n"
	var wr := VCSVParser.parse_string(wsrc, null)
	var weapons := VCSVDataTable.new()
	weapons.headers = wr.table.headers
	weapons.rows = wr.table.rows
	weapons.key_column = "id"
	weapons.row_type = "res://scripts/row_types/weapon_row.gd"
	weapons.set_linked_table("monsters", mpath)

	var issues := weapons.validate()
	check(has_issue(issues, "unresolved foreign key 'ghost'"), "validate reports missing FK reference")
	check(not has_issue(issues, "unresolved foreign key 'h1'"), "valid FK reference not reported")


func _init() -> void:
	test_required_columns()
	test_conversion_errors()
	test_duplicate_ids()
	test_fk_missing()
	if failures == 0:
		print("test_validation OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_validation FAILED: ", failures, "/", checks, " failed")
		quit(1)
