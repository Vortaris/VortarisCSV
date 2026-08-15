# Data types

Cells are plain strings after parsing. `VCSVDataTable` converts each cell to
the target property's declared type; `VCSVUtil.detect_types` / 
`load_csv_dict_array` infer types heuristically.

## Supported target types

| Group | Types |
|---|---|
| Scalars | `int`, `float`, `bool`, `String`, `StringName`, `NodePath` |
| Enum | any `int` property with an enum hint — cell can be the numeric value or the name (case-insensitive), e.g. `3` or `melee` |
| Vectors / geometry | `Vector2`, `Vector2i`, `Vector3`, `Vector3i`, `Vector4`, `Vector4i`, `Rect2`, `Rect2i`, `Plane`, `AABB` |
| Transforms | `Quaternion`, `Basis`, `Transform2D`, `Transform3D`, `Projection` |
| Color | `#rrggbb`, `#rrggbbaa`, `Color(r,g,b[,a])`, `r,g,b[,a]`, or a named color |
| Arrays | `Array[T]` / `Array` and `Packed*Array` — split by `array_delimiter` (default `;`); JSON array literals (`[1,2,3]`) and a mix of both in one column also bind (v0.2.0) |
| JSON | `Dictionary` cells, or a column typed `"json"` — cell is parsed with `JSON.parse_string` |
| Object (foreign key) | `OBJECT` property → resolved via `linked_tables` (v1) |

## Syntax examples

```
hp=100                 int
attack=1.5             float
alive=true             bool
element=melee          enum (matches the enum name)
position=10,20         Vector2
color=#ff8000          Color
tags=1;2;3             Array[int]
tags=[1,2,3]           Array[int] (JSON literal, v0.2.0)
weaknesses=fire;ice    Array[String]
data={"dmg":5}         Dictionary (JSON)
owner=goblin           OBJECT → linked table row "goblin"
```

`Vector2(10, 20)` and `Color(1, 0.5, 0, 1)` wrappers are also accepted.

## Empty / null handling

- Empty cells and cells equal to `null_token` keep the property's default value.
- A cell that fails to parse is recorded in `get_last_errors()` (with row/col)
  and the property keeps its default; the build continues.

## `column_types` overrides

Set `VCSVDataTable.column_types` to force a column's target type by name,
overriding the property's declared type:

```gdscript
table.column_types = { "some_int_col": "int", "some_array_col": "int[]", "cfg": "json" }
```

Canonical names: `bool int float string StringName NodePath Vector2 Vector2i
Vector3 Vector3i Vector4 Vector4i Rect2 Rect2i Color AABB Plane Quaternion Basis
Transform2D Transform3D Projection` plus array forms `int[] float[] bool[]
string[]` and `json`.

## Header schema (`hp:int`, v0.2.0)

With `VCSVParseOptions.header_type_separator` set (default off), a header cell of
the form `name<sep>Type` is split at parse time: the annotation is stripped from
the header name and the declared type is exposed via `VCSVParseResult.column_types`
and honored by `VCSVUtil.load_csv_dict_array`. This lets a CSV declare its own
column types without a separate config:

```gdscript
var opts := VCSVParseOptions.new()
opts.header_type_separator = ":"
var result := VCSVParser.parse_file("res://data/monsters.csv", opts)
print(result.table.headers)   # ... , "hp", ...   (annotation stripped)
print(result.column_types)    # {"hp": "int", ...}
```

Invalid type names (e.g. `hp:notatype`) strip the annotation and fall back to
`string`. In the editor import panel the equivalent is the explicit
`column_types` text field (`hp:int;attack:float`), which wins over detection.

## Custom converter

For anything the built-in conversion can't express, set a `Callable`:

```gdscript
table.set_cell_converter(func(column: String, cell: String, prop: Dictionary, row: Resource) -> Variant:
    if column == "note":
        return "NOTE: " + cell
    return null  # null → use the built-in conversion
)
```

## Encoding

`VCSVParseOptions.encoding` (and the import-panel `encoding` option) selects the
file encoding read by `VCSVParser.parse_file` / the editor import:

- `"utf8"` (default) — with BOM stripping.
- `"gbk"` / `"gb2312"` — decodes legacy Chinese-encoded files. Covers the
  GB2312 core and common GBK characters (lead byte 0xA1-0xF7); unknown bytes
  become U+FFFD.

## Aggregations

`column_stats(column)` returns `{count, non_empty, numeric, min, max, sum, avg,
distinct}` over a column (non-numeric cells are counted but excluded from the
numeric fields):

```gdscript
var s: Dictionary = table.column_stats("hp")
print(s.min, "..", s.max, " avg=", s.avg, " distinct=", s.distinct)
```

## Batch editing

```gdscript
table.add_rows([[ "x", "1" ], PackedStringArray(["y", "2"])])
dt.append_dicts([{ "id": "k9", "hp": 999 }])   # aligned to dt.headers
dt.set_row_dict("k9", { "hp": "500", "name": "boss" })
```

## Cross-table queries (joins)

`linked_tables` + an OBJECT property resolve foreign keys; the query helpers
build on them:

```gdscript
# WeaponRow has `@export var owner: MonsterRow`; weapons.linked_tables = {"monsters": "res://...tres"}
var related: MonsterRow = weapons.get_related("orc", "monsters")
var joined := weapons.join_rows("monsters")   # flat dicts, related cols prefixed "monsters."
```

> **Note on refresh:** typed rows cache their resolved foreign-key objects at build
> time. If you mutate a *linked* table (e.g. `set_rows` + `refresh()`), call
> `refresh()` on the referencing table too so its cached FK references are
> rebuilt.

## Type inference (no row type)

`VCSVUtil.detect_types(table, array_delimiter=";", detect_booleans=false)`
returns a `header → type-name` dictionary. Inference recognizes int / float /
bool (`true`/`false`, when enabled) / comma-separated vectors / `#hex` colors /
`array_delimiter` arrays / JSON object & array literals; anything else is
`string`. `load_csv_dict_array` uses inference (with booleans enabled) to return
typed `Dictionary` rows.
