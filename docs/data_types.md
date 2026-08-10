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
| Arrays | `Array[T]` / `Array` and `Packed*Array` — split by `array_delimiter` (default `;`) |
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

## Custom converter

For anything the built-in conversion can't express, set a `Callable`:

```gdscript
table.set_cell_converter(func(column: String, cell: String, prop: Dictionary, row: Resource) -> Variant:
    if column == "note":
        return "NOTE: " + cell
    return null  # null → use the built-in conversion
)
```

## Type inference (no row type)

`VCSVUtil.detect_types(table, array_delimiter=";", detect_booleans=false)`
returns a `header → type-name` dictionary. Inference recognizes int / float /
bool (`true`/`false`, when enabled) / comma-separated vectors / `#hex` colors /
`array_delimiter` arrays / JSON object & array literals; anything else is
`string`. `load_csv_dict_array` uses inference (with booleans enabled) to return
typed `Dictionary` rows.
