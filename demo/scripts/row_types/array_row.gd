class_name ArrayRow
extends Resource

# Row type with several array-typed properties, used by
# test_datatable_script.test_typed_array_forms to cover the various array cell
# forms (JSON array literal, ";"-separated string, mixed, Array[String]).

@export var id: String = ""
@export var ints: Array[int] = []
@export var strs: Array[String] = []
@export var mixed: Array[int] = []
