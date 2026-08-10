class_name WeaponRow
extends Resource

# Row type with a foreign-key property: `owner` resolves to a MonsterRow via
# VCSVDataTable.linked_tables (see test_features.gd).

@export var id: String = ""
@export var name: String = ""
@export var owner: MonsterRow = null
