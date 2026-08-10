class_name MonsterRow
extends Resource

# Example row type bound to CSV columns by name (see test_datatable_script.gd).

@export var id: String = ""
@export var name: String = ""
@export var health: int = 0
@export var attack: float = 0.0
@export var alive: bool = false
@export var position: Vector2 = Vector2.ZERO
@export var color: Color = Color.WHITE
@export var tags: Array[int] = []
@export var notes: Dictionary = {}
@export var box: AABB = AABB()
