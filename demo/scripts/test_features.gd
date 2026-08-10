extends SceneTree

# Regression tests for the 0.1.1 feature batch: GBK encoding, aggregations,
# batch APIs and cross-table joins.
# Run: Godot --headless --path demo --script res://scripts/test_features.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func test_gbk_encoding() -> void:
	# "名字,血量\n哥布林,100\n" encoded as GBK (你 uses 0xC4E3 etc.).
	var gbk := PackedByteArray([
		0xC3, 0xFB, 0xD7, 0xD6, 0x2C, 0xD1, 0xAA, 0xC1, 0xBF, 0x0A,
		0xB8, 0xE7, 0xB2, 0xBC, 0xC1, 0xD6, 0x2C, 0x31, 0x30, 0x30, 0x0A,
	])
	var path := "user://test_gbk.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_buffer(gbk)
	f.close()

	# Without encoding set, GBK bytes mis-decode as UTF-8.
	var opts := VCSVParseOptions.new()
	opts.encoding = "gbk"
	var r := VCSVParser.parse_file(path, opts)
	check(r.success, "gbk parse succeeds")
	check(r.table.headers[0] == "名字", "gbk header decoded")
	check(r.table.get_value(0, "名字") == "哥布林", "gbk cell decoded")
	check(r.table.get_value(0, "血量") == "100", "gbk numeric cell")

	# BOM/utf8 path still works.
	var utf8_path := "user://test_utf8.csv"
	var f2 := FileAccess.open(utf8_path, FileAccess.WRITE)
	f2.store_string("名字,血量\n兽人,50\n")
	f2.close()
	var r2 := VCSVParser.parse_file(utf8_path, null)
	check(r2.success and r2.table.get_value(0, "名字") == "兽人", "utf8 still fine")


func test_column_stats() -> void:
	var t := VCSVParser.parse_string("id,hp\nk1,100\nk2,50\nk3,10\nk4,50\nk5,x\n", null).table
	var s := t.column_stats("hp")
	check(s["count"] == 5, "stats count")
	check(s["non_empty"] == 5, "stats non_empty")
	check(s["numeric"] == true, "stats numeric")
	check(s["min"] == 10, "stats min")
	check(s["max"] == 100, "stats max")
	check(s["sum"] == 210, "stats sum")
	check(s["avg"] == 52.5, "stats avg (210/4)")
	check(s["distinct"] == 4, "stats distinct (50 twice)")


func test_batch() -> void:
	var r := VCSVParser.parse_string("id,hp\nk1,10\n", null)
	var t := VCSVDataTable.new()
	t.headers = r.table.headers
	t.rows = r.table.rows
	t.key_column = "id"
	t.row_type = "res://scripts/row_types/monster_row.gd"

	# append_dicts
	t.append_dicts([{"id": "k2", "hp": 20}, {"id": "k3", "hp": 30}])
	check(t.row_count() == 3, "append_dicts grows")
	check(t.get_row_dict("k3")["hp"] == "30", "append_dicts values")

	# set_row_dict (batch set several cells of one row)
	t.set_row_dict("k2", {"hp": "999"})
	check(t.get_row_dict("k2")["hp"] == "999", "set_row_dict edits grid")
	var row: MonsterRow = t.get_row("k2")
	check(row != null, "set_row_dict keeps row usable")

	# add_rows on the table level
	var vt := VCSVParser.parse_string("a,b\n1,2\n", null).table
	vt.add_rows([["3", "4"], PackedStringArray(["5", "6"])])
	check(vt.get_row_count() == 3, "add_rows count")
	check(vt.get_row(2) == PackedStringArray(["5", "6"]), "add_rows mixed forms")


func test_join() -> void:
	# Monsters table (the linked target).
	var msrc := "id,name,health\n"
	msrc += "goblin,哥布林,100\n"
	msrc += "orc,兽人,80\n"
	var mr := VCSVParser.parse_string(msrc, null)
	var monsters := VCSVDataTable.new()
	monsters.headers = mr.table.headers
	monsters.rows = mr.table.rows
	monsters.key_column = "id"
	monsters.row_type = "res://scripts/row_types/monster_row.gd"
	var mpath := "user://test_monsters.tres"
	ResourceSaver.save(monsters, mpath)

	# Weapons table with an FK column "owner".
	var wsrc := "id,name,owner\nw1,sword,goblin\nw2,axe,orc\n"
	var wr := VCSVParser.parse_string(wsrc, null)
	var weapons := VCSVDataTable.new()
	weapons.headers = wr.table.headers
	weapons.rows = wr.table.rows
	weapons.key_column = "id"
	weapons.row_type = "res://scripts/row_types/weapon_row.gd"
	weapons.set_linked_table("monsters", mpath)

	# FK resolved on the typed row.
	var w1: WeaponRow = weapons.get_row("w1")
	check(w1 != null, "weapon row built")
	check(w1.owner != null, "owner resolved via FK")
	if w1 and w1.owner:
		check(w1.owner.health == 100, "owner health via FK")
		check(w1.owner.name == "哥布林", "owner name via FK")

	# Explicit related lookup.
	var related: MonsterRow = weapons.get_related("orc", "monsters")
	check(related != null and related.health == 80, "get_related")
	var related_dict := weapons.get_related_dict("orc", "monsters")
	check(related_dict["name"] == "兽人", "get_related_dict")

	# join_rows: merged dicts with the related table prefixed.
	var joined := weapons.join_rows("monsters")
	check(joined.size() == 2, "join_rows count")
	var j0: Dictionary = joined[0]
	check(j0["name"] == "sword", "join keeps local name")
	check(j0["monsters.health"] == 100, "join flattens related health")
	check(j0["monsters.name"] == "哥布林", "join flattens related name")


func _init() -> void:
	test_gbk_encoding()
	test_column_stats()
	test_batch()
	test_join()
	if failures == 0:
		print("test_features OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_features FAILED: ", failures, "/", checks, " failed")
		quit(1)
