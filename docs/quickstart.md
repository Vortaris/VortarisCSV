# Quickstart

VortarisCSV gives you three levels of use. Start here.

**Demo data** used throughout this guide (`res://data/monsters.csv` in the demo
project):

```csv
id,name,health,attack,alive,position,color,tags,notes
goblin,哥布林,100,1.5,true,"10,20",#ff0000,"1;2;3",{"weak":"fire"}
orc,兽人,80,2.0,false,"30,40",#00ff00,"4;5",{"weak":"ice"}
```

## 1. One-liner: typed Array[Dictionary]

No row class needed — cells are converted by inferred column types:

```gdscript
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
var goblin: Dictionary = rows[0]
print(goblin.health, " ", goblin.position)   # 100  (10, 20)
print(typeof(goblin.health), " ", typeof(goblin.tags))  # TYPE_INT  TYPE_ARRAY
```

- `VCSVUtil.load_csv_dict("res://data/monsters.csv")` returns only the first data
  row as a typed `Dictionary` (single-row variant, v0.2.0).
- Both return an empty result on failure (missing file, parse error), so check
  `is_empty()` before indexing.
- Type inference recognizes `int`, `float`, `bool` (`true`/`false`),
  comma-separated vectors (`10,20` → `Vector2`), `#hex` colors, `;`-separated
  arrays, and JSON `{...}`/`[...]` cells; everything else is a `String`.

Pass a `VCSVParseOptions` as the second argument to control parsing:

```gdscript
var opts := VCSVParseOptions.new()
opts.delimiter = ";"
opts.encoding = "gbk"                       # for legacy Chinese-encoded files
var rows := VCSVUtil.load_csv_dict_array("res://data/monsters.csv", opts)
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

Notes on the low-level layer:

- `VCSVTable` holds **raw strings** — no type conversion. `headers` is the first
  row when `has_header` is true; `rows` are data rows only.
- Access by index or by header name: `table.get_value(0, "name")` and
  `table.get_value(0, 2)` are equivalent.
- `VCSVParseResult` carries structured errors: on failure `result.success` is
  `false`, `result.error` is a Godot `Error` code (`ERR_PARSE_ERROR`,
  `ERR_FILE_NOT_FOUND`, ...), and `result.error_line` / `result.error_column`
  are 1-based. Non-fatal issues (e.g. lenient row padding) land in
  `result.warnings`.

`VCSVWriter` serializes back:

```gdscript
var w := VCSVWriter.new()
w.line_ending = "\n"
var err := w.write_table(table, "user://copy.csv")   # err == OK on success
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

### The hot-path variant (v0.3.1)

`load_typed` / `load_csv_typed` parse the CSV and bind the row type **once**.
Cache the returned table so every `get_row()` is a cache hit:

```gdscript
var typed: VCSVDataTable = VCSVDataTable.load_typed(
    "res://data/monsters.csv", "res://scripts/row_types/monster_row.gd")
var goblin: MonsterRow = typed.get_row("goblin")   # fast; no per-lookup rebuild
var tags: Array = typed.get_field_array("goblin", "tags")   # native Array
```

Without this, the anti-pattern `from_dict_array([dict], row_type).get_row(id)`
rebuilds a whole table on every hot-path lookup.

## CSV → row-type mapping

- Column **names** map to properties by name (see `case_insensitive_columns`).
- Cell strings are converted to each property's declared type: `int`, `float`,
  `bool`, `String`, enums, `Vector2/3/4`, `Color`, `Rect2`, arrays (split by
  `array_delimiter`, default `;`), JSON `Dictionary` cells, and more.
- Empty cells / `null_token` keep the property's default.
- A column with no matching property is ignored (a warning is recorded);
  a property with no matching column keeps its default.
- Header schema (v0.2.0): with `VCSVParseOptions.header_type_separator` set, a
  column named `hp:int` maps to property `hp` and forces type `int`.

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

Numeric vs lexicographic sorting matters: lexicographically `"10" < "100" < "50"`;
numerically `10 < 50 < 100`. Pass `numeric = true` to compare as numbers.

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

## 6. Validate, delta export & big-file options (v0.2.x)

```gdscript
var dt: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")

# Data-integrity checks: missing required columns, conversion failures,
# duplicate keys, unresolved foreign keys. Empty PackedStringArray = clean.
var issues: PackedStringArray = dt.validate({"required_columns": ["id", "name"]})
print("issues: ", issues)

# Delta export: write only the rows whose keys are given (headers included).
dt.export_rows_to_csv(["goblin", "orc"], "user://subset.csv")
```

For large files: slice the parse with `VCSVParseOptions.row_offset` / `max_rows`,
and defer typed-row construction with `lazy_build = true` (structure-only build,
rows constructed on demand by `get_row` / `build_row`). With a `source_path` +
`hot_reload` set, the editor plugin re-parses the CSV automatically when it
changes on disk (see `docs/AI_DEBUGGING.md` for the headless CLI).

## Editor main screen in 30 seconds

1. Enable **VortarisCSV** in *Project → Project Settings → Plugins*.
2. Drop a `.csv` into `res://` and double-click it in the FileSystem dock.
3. The **CSV** tab opens the table. Double-click a cell to edit; the change is
   written back to the `.csv` and reimported automatically.
4. Use **Import CSV / Export CSV / Export Rows** in the toolbar, and read the
   details/validation panel on the right.

See `docs/import_pipeline.md` for the full editor workflow.

## Run the tests

```sh
godot --headless --path demo --script res://scripts/test_parser.gd
godot --headless --path demo --script res://scripts/test_datatable_script.gd
# ...every test_*.gd exits 0 on success
```

> **Fresh clone?** The GDExtension cache `.godot/extension_list.cfg` is
> gitignored. Run `godot --headless --editor --import --quit --path demo` once
> (or open the project in the editor) before `--script` tests, or the `VCSV*`
> classes won't load and tests will fail with "Identifier not declared".
