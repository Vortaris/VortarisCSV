extends SceneTree

# Walkthrough of the three usage levels. Run:
#   Godot --headless --path demo --script res://scripts/quickstart.gd

func _init() -> void:
	# 1) One-liner: typed Array[Dictionary] (CSVAccess-style).
	var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
	print("== typed dicts ==")
	print("count=", rows.size(), " first=", rows[0])

	# 2) Low-level parse to a string grid.
	var result := VCSVParser.parse_file("res://data/monsters.csv", null)
	if result.success:
		var table: VCSVTable = result.table
		print("== raw grid ==")
		print("headers=", table.headers)
		print("row0=", table.get_row(0))
		print("cell(0, name)=", table.get_value(0, "name"))

	# 3) UE-DataTable style: bind rows to a typed class.
	var table: VCSVDataTable = VCSVDataTable.from_file(
			"res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
	var goblin: MonsterRow = table.get_row("goblin")
	print("== typed rows ==")
	if goblin:
		print("goblin: hp=", goblin.health, " pos=", goblin.position,
				" color=", goblin.color, " tags=", goblin.tags)
		var notes: Dictionary = goblin.notes
		print("weakness=", notes.get("weak", "-"))
	else:
		print("no goblin (is data/monsters.csv imported / readable?)")

	print("keys=", table.get_keys())
	quit(0)
