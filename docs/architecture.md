# Architecture

VortarisCSV is a three-layer Godot GDExtension (godot-cpp) library. The pure
C++ core never allocates per character and never touches `Variant` on hot paths.

```
┌─ Low level (string layer, fast & flexible) ──────────────┐
│  VCSVParser ──► VCSVParseResult ──► VCSVTable             │
│  VCSVWriter (serialization)                              │
├─ Middle (internal C++: vortariscsv::) ───────────────────┤
│  csv_parser / csv_writer / type_converter / type_inference
│  column_index / reflection_binder / row_factory           │
├─ High level (object layer, easy to use) ─────────────────┤
│  VCSVDataTable (UE-DataTable-style reflection binding)    │
│  VCSVUtil (static one-liners)                            │
└─ Editor layer ────────────────────────────────────────────┘
   VCSVEditorImportPlugin (.csv/.tsv → .tres)
```

## Source layout

| Directory | Contents |
|---|---|
| `src/core/` | Pure C++ algorithms (`namespace vortariscsv`), no Godot objects |
| `src/reflect/` | Reflection binding + row factory |
| `src/gdscript/` | `VCSV`-prefixed Godot classes (thin bindings) |
| `src/editor/` | `VCSVEditorImportPlugin` (registered at EDITOR init) |
| `src/demo/` | `DemoMonsterRow` C++ row type used by the test suite |
| `doc_classes/` | godot-docs XML, compiled into the DLL (editor/template_debug) |

## Parsing

A single-pass RFC 4180 state machine (`OUTSIDE` / `IN_QUOTES` / `AFTER_QUOTES`)
walks a `char32_t*` pointer. Fields are accumulated in a `std::vector<char32_t>`
and materialized with the engine's `string_new_with_utf32_chars` constructor —
`resize()`+`ptrw()`+`memcpy` on a COW `String` is unreliable, so it is avoided.
BOM is stripped at the byte level (read `PackedByteArray`, skip `EF BB BF`, then
`String::utf8`). Errors carry a 1-based line/column.

## Two-pass column typing

Pass 1 classifies every non-empty cell (int / float / bool / vector / Color /
array-of-X / JSON / string); per-column unification collapses `INT+FLOAT → FLOAT`,
any `STRING` wins, mixed arrays → `string[]`. Pass 2 converts with
`String → Variant` against the unified type. With a `row_type` present, the
target types come from the row object's declared properties instead; the
inferred `column_types` are baked into the imported `.tres` for inspection and
overrides.

## Reflection binding

`VCSVDataTable.ensure_loaded()`:
1. Instantiates the row type (`Script::new()` or `ClassDB::instantiate`).
2. Reads `Object::get_property_list()`, filters by usage
   (`STORAGE | SCRIPT_VARIABLE`, excluding `INTERNAL/GROUP/CATEGORY/SUBGROUP`
   and `Resource`'s own properties).
3. Maps header columns → property names (case-insensitive option).
4. For each row, converts every cell against the property's `PropertyInfo`
   (or a `column_types` override, or a user `Callable` converter) and
   `Object::set()`s it. Failing cells keep the default and are recorded.

Only the string grid + config is serialized; row objects are always rebuilt
from the *current* `row_type` script, so hot-reload re-binds automatically.

## Foreign keys (v1, simplified)

A property typed `SomeRow` (OBJECT) whose cell holds another table's key is
resolved lazily via `linked_tables`: `resolve_object` finds the linked
`VCSVDataTable` whose row-type class name matches the property's class name and
calls its `get_row(cell)`. Cross-table cycles are the user's responsibility in
v1 (a re-entrant build guard leaves unresolved cells null).

## Performance notes

- Single-pass parse, O(n); per-field allocation, never per-char.
- Two-pass typing avoids the O(n²) retroactive rewrites in the GDScript plugins.
- `VCSVDataTable` builds rows once and caches; lookups are O(1) via the key map.
- The pure C++ core returns raw `PackedStringArray` grids; `Variant` is only
  touched in the typed layers, off hot paths.
