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
| Object (foreign key) | `OBJECT` property → resolved via `linked_tables` |

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

`Vector2(10, 20)` and `Color(1, 0.5, 0, 1)` wrappers are also accepted, as are
nested literals like `AABB(Vector3(1,2,3), Vector3(4,5,6))`.

## Exact conversion rules

Per target type (implemented in `src/core/type_converter.cpp`):

- **`int`** — the cell must be a valid integer (`is_valid_int()`). `"100"` → `100`;
  `"1.5"` fails and is recorded as an error.
- **`float`** — the cell must be a valid float. `"1.5"` → `1.5`; `"50"` → `50.0`.
- **`bool`** — case-insensitive truthy/falsy sets: `true`/`1`/`yes`/`on` → `true`;
  `false`/`0`/`no`/`off` → `false`. Anything else is an error.
- **`String` / `StringName` / `NodePath`** — verbatim cell value.
- **Enum** — a numeric cell (`3`) is used directly; otherwise the cell is matched
  against the hint names **case-insensitively** (`melee` → the value of `Melee`).
  Hint strings in either `Name,Name2` or `Value:Name` form are handled.
- **Vectors / geometry / transforms** — comma-separated numbers, with the exact
  component count enforced:
  | Type | Count | Meaning |
  |---|---|---|
  | `Vector2` / `Vector2i` | 2 | x, y |
  | `Vector3` / `Vector3i` | 3 | x, y, z |
  | `Vector4` / `Vector4i` | 4 | x, y, z, w |
  | `Rect2` / `Rect2i` | 4 | position (x, y), size (w, h) |
  | `Plane` | 4 | normal (x, y, z), d |
  | `Quaternion` | 4 | x, y, z, w |
  | `AABB` | 6 | origin (x, y, z), size (x, y, z) |
  | `Basis` | 9 | 3 column vectors (row-major) |
  | `Transform2D` | 6 | columns[0], columns[1], origin |
  | `Transform3D` | 12 | basis (9) + origin (3) |
  | `Projection` | 16 | 4 column vectors (row-major) |
  A wrong component count is a conversion error (property keeps its default).
- **`Color`** — one of: `#rrggbb`, `#rrggbbaa` (hex, alpha allowed),
  `r,g,b[,a]` (each 0..1), a `Color(...)` wrapper, or a **named color**
  (`red`, `white`, `black`, ...). A hex with `alpha==0` still parses correctly.
- **Arrays** — see below.
- **`Dictionary`** — the cell is parsed as JSON; a valid JSON object (`{...}`)
  is used directly. A JSON array or invalid JSON is an error.
- **Packed arrays** (`PackedInt32Array`, `PackedVector2Array`, ...) — split by
  `array_delimiter`; each element must parse as the element type.
- **`OBJECT`** — the cell is treated as a foreign key into a linked table (see
  "Cross-table queries"). An unresolvable key is recorded as an error and the
  property keeps `null`.

### Array-typed properties

An `Array[T]` or `Packed*Array` property accepts three cell forms, and a single
column can even mix them across rows (v0.2.0):

```
tags=1;2;3        ;-separated strings     -> Array[int]   [1, 2, 3]
tags=[1,2,3]      JSON array literal      -> Array[int]   [1, 2, 3]
mixed=1;2         ;-form per row          -> Array[int]
```

- For `Array[T]`, each element is converted to `T`. `Array[int]` from `"1;2;3"`
  gives `[1, 2, 3]`; `Array[String]` from `"fire;ice"` gives `["fire", "ice"]`.
- A JSON literal `[1,2,3]` inside a quoted CSV field also works
  (`"\"[1,2,3]\""` in the raw CSV, or just `"[1,2,3]"` in a `VCSVParser.parse_string` source).
- Empty elements are preserved as `null`/empty slots.
- `VCSVDataTable.get_field_array(key, field)` (v0.3.1) returns the field's
  native `Array` directly — no manual `split(";")` needed.

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

Canonical names are resolved **case-insensitively** and accept any Godot Variant
type name plus array forms:

```
Scalars:  bool  int  float  string  StringName  NodePath
Geometry: Vector2  Vector2i  Vector3  Vector3i  Vector4  Vector4i
          Rect2  Rect2i  Color  AABB  Plane
Transforms: Quaternion  Basis  Transform2D  Transform3D  Projection
Arrays:   int[]  float[]  bool[]  string[]  (any Variant type name + "[]")
JSON:     json   (cell is parsed with JSON.parse_string)
```

Notes:
- `column_types` overrides win over the property's declared type (and over
  `detect_types` on import).
- An invalid override name is ignored (the property's own type is used) and a
  warning is recorded.

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

Contract:
- The callable receives `(column_name, cell_string, property_dict, row_object)`.
- Returning a **non-null** value uses it as the property value. For typed-array
  properties, a native `Array` you return is coerced to the declared element type.
- Returning **null** falls back to the built-in conversion.
- Returning a value that can't be assigned to the property produces a
  conversion error (recorded in `get_last_errors()`).

## Encoding

`VCSVParseOptions.encoding` (and the import-panel `encoding` option) selects the
file encoding read by `VCSVParser.parse_file` / the editor import:

- `"utf8"` (default) — with BOM stripping.
- `"gbk"` / `"gb2312"` — decodes legacy Chinese-encoded files. Covers the
  GB2312 core and common GBK characters (lead byte 0xA1-0xF7); unknown bytes
  become U+FFFD.

The BOM is stripped at the **byte level** before decoding, so a UTF-8 BOM in a
GBK file doesn't pollute the first header.

## Aggregations

`column_stats(column)` returns `{count, non_empty, numeric, min, max, sum, avg,
distinct}` over a column (non-numeric cells are counted but excluded from the
numeric fields):

```gdscript
var s: Dictionary = table.column_stats("hp")
print(s.min, "..", s.max, " avg=", s.avg, " distinct=", s.distinct)
```

- `min` / `max` / `sum` / `avg` are present **only** when at least one numeric
  cell exists.
- `numeric` is `true` when every non-empty cell parsed as a number.
- `distinct` is the count of unique cell values (by string).

## Batch editing

```gdscript
table.add_rows([[ "x", "1" ], PackedStringArray(["y", "2"])])   # VCSVTable
dt.append_dicts([{ "id": "k9", "hp": 999 }])   # aligned to dt.headers
dt.set_row_dict("k9", { "hp": "500", "name": "boss" })
```

- `VCSVTable.add_row(values)` appends one row (elements stringified) and returns
  its index; `add_rows` appends many at once.
- `VCSVDataTable.append_dicts` aligns each dict to the table's headers — unknown
  keys are ignored, missing keys are empty.
- `VCSVDataTable.set_row_dict` sets several cells of one row (by key) and
  rebuilds the typed cache.

## Cross-table queries (joins)

`linked_tables` + an OBJECT property resolve foreign keys; the query helpers
build on them:

```gdscript
# WeaponRow has `@export var owner: MonsterRow`; weapons.linked_tables = {"monsters": "res://...tres"}
var related: MonsterRow = weapons.get_related("orc", "monsters")
var joined := weapons.join_rows("monsters")   # flat dicts, related cols prefixed "monsters."
```

- `get_related(key, table_name)` — the linked table's typed row for `key`.
- `get_related_dict(key, table_name)` — string-level variant.
- `join_rows(table_name)` — merges every typed row that has an OBJECT property
  bound to the linked table's row type with its resolved related row, returning
  flat dicts where related columns are prefixed `<table_name>.`.
- A `VCSVDataTable` property typed `SomeRow` resolves to the linked table whose
  **row-type class name** matches (`SomeRow`); it calls that table's `get_row(cell)`.

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

Unification rules (two-pass):
- `INT + FLOAT` in the same column → `float`.
- Any `string` cell → `string`.
- Mixed array element types → `string[]`.
- Empty columns infer as `string`.

## Column lookup by name or index

Every API that takes a `column` accepts either an `int` column index or a
`String` header name — e.g. `table.get_value(0, "hp")`, `table.get_value(0, 2)`,
`table.sort(1, true, true)`, `table.column_stats("hp")`. An unknown header name
resolves to index -1 and is a no-op / error depending on the method.
