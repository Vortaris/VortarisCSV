# Quickstart

VortarisCSV gives you three levels of use. Start here.

## 1. One-liner: typed Array[Dictionary]

No row class needed — cells are converted by inferred column types:

```gdscript
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
var goblin: Dictionary = rows[0]
print(goblin.health, " ", goblin.position)   # 100  (10, 20)
```

## 2. Low-level: parse to a string grid

```gdscript
var opts := VCSVParseOptions.new()
opts.delimiter = ","
opts.comment_prefix = "#"

var result := VCSVParser.parse_file("res://data/monsters.csv", opts)
if not result.success:
    push_error("Line %d:%d: %s" % [result.error_line, result.error_column, result.message])
    return

var table: VCSVTable = result.table
for row in table.get_rows():
    print(row)                       # PackedStringArray of cells
```

`VCSVWriter` serializes back:

```gdscript
var w := VCSVWriter.new()
w.line_ending = "\n"
w.write_table(table, "user://copy.csv")
```

## 3. UE-DataTable style: bind rows to a typed class

Define a row type (must extend `Resource`):

```gdscript
class_name MonsterRow
extends Resource

@export var id: String = ""
@export var name: String = ""
@export var health: int = 0
@export var attack: float = 0.0
@export var alive: bool = false
@export var position: Vector2 = Vector2.ZERO
@export var color: Color = Color.WHITE
@export var tags: Array[int] = []
```

Load and query:

```gdscript
var table: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")

var goblin: MonsterRow = table.get_row("goblin")   # key column = first header ("id")
print(goblin.health, " ", goblin.position, " ", goblin.color)
```

Or use the editor: enable the **VortarisCSV** plugin in *Project → Plugins*, and
`.csv`/`.tsv` files in your project are imported as `VCSVDataTable` resources.
Then:

```gdscript
var table: VCSVDataTable = load("res://data/monsters.csv")
```

## CSV → row-type mapping

- Column **names** map to properties by name (see `case_insensitive_columns`).
- Cell strings are converted to each property's declared type: `int`, `float`,
  `bool`, `String`, enums, `Vector2/3/4`, `Color`, `Rect2`, arrays (split by
  `array_delimiter`, default `;`), JSON `Dictionary` cells, and more.
- Empty cells / `null_token` keep the property's default.
- A column with no matching property is ignored (a warning is recorded);
  a property with no matching column keeps its default.

See `docs/data_types.md` for the full type matrix and `column_types` overrides.

## 4. Search & sort

```gdscript
# String-level: sort the grid (numeric-aware) and search cells.
var table: VCSVTable = result.table
table.sort("hp", true, true)                 # ascending, numeric
var rows := table.find("name", "gob", VCSVTable.MATCH_CONTAINS)  # row indices

# Typed level: sort re-binds rows; find returns keys; filter runs a predicate.
var dt: VCSVDataTable = ...                  # from_file / load
dt.sort_rows("health", false, true)          # descending by health
var keys := dt.find_rows("color", "#ff", VCSVTable.MATCH_PREFIX)
var strong := dt.filter(func(row): return row.health > 50)
```

`MatchMode`: `MATCH_EXACT`, `MATCH_NOCASE_EXACT`, `MATCH_CONTAINS`,
`MATCH_NOCASE_CONTAINS`, `MATCH_PREFIX`, `MATCH_NOCASE_PREFIX`.

## 5. JSON / dictionary interop

```gdscript
# Table → typed dictionaries → JSON.
var dicts: Array = table.to_dict_array()      # VCSVTable or VCSVDataTable
var json: String = table.to_json_string()
save_to_file(json)

# JSON / dictionaries → table.
var t1 := VCSVTable.from_dict_array(dicts)
var t2 := VCSVDataTable.from_json_string(json, "res://scripts/row_types/monster_row.gd")
var goblin: MonsterRow = t2.get_row("goblin")
```

Round trips preserve column order and cell types (JSON has no int/float split;
integral numbers re-bind to either). `VCSVUtil.table_to_dict_array(table)` and
`VCSVUtil.load_csv_dict_array(path)` give the same typed-dict views directly.

## Run the tests

```sh
Godot --headless --path demo --script res://scripts/test_parser.gd
Godot --headless --path demo --script res://scripts/test_datatable_script.gd
# ...every test_*.gd exits 0 on success
```
