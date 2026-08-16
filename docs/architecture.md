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

The split exists so the hot paths (parsing a giant file, converting cells) run
in plain C++ with no `Variant` churn; `Variant` only appears in the typed layers
where the row objects are built.

## Source layout

| Directory | Contents |
|---|---|
| `src/core/` | Pure C++ algorithms (`namespace vortariscsv`), no Godot objects |
| `src/reflect/` | Reflection binding + row factory |
| `src/gdscript/` | `VCSV`-prefixed Godot classes (thin bindings) |
| `src/editor/` | `VCSVEditorImportPlugin` (registered at EDITOR init) |
| `src/demo/` | `DemoMonsterRow` C++ row type used by the test suite |
| `src/register_types.cpp` | GDExtension entry (`vortariscsv_library_init`) |
| `doc_classes/` | godot-docs XML, compiled into the DLL (editor/template_debug) |

### Core modules (`src/core/`)

- `csv_parser` — RFC 4180 state machine (`OUTSIDE` / `IN_QUOTES` / `AFTER_QUOTES`).
- `csv_writer` — quoting / line endings / formula-injection guard.
- `type_converter` — `String → Variant` against a `PropertyInfo` (or bare
  `Variant::Type`), plus `parse_enum`, `parse_color`, nested literal parsing and
  typed-array coercion.
- `type_inference` — two-pass column typing (`detect_cell_type` +
  `infer_column_types`).
- `column_index` — header → index map with optional case-insensitive lookup.
- `gbk` — GBK/GB2312 byte decoding (GB2312 core + common GBK range).
- `vcsv_log` — gated logging macros (`log_info` debug-only, `log_verbose`
  debug + `vortariscsv/general/verbose`).
- `vcsv_settings` — `get_setting_with_fallback()` (new path, then old flat path).

## Parsing

A single-pass RFC 4180 state machine (`OUTSIDE` / `IN_QUOTES` / `AFTER_QUOTES`)
walks a `char32_t*` pointer. Fields are accumulated in a `std::vector<char32_t>`
and materialized with the engine's `string_new_with_utf32_chars` constructor —
`resize()`+`ptrw()`+`memcpy` on a COW `String` is unreliable, so it is avoided.
BOM is stripped at the byte level (read `PackedByteArray`, skip `EF BB BF`, then
`String::utf8`). Errors carry a 1-based line/column.

Parsing rules worth knowing:

- `""` inside quotes is one literal quote.
- The delimiter, `\r`, and `\n` inside quotes are literal (quoted fields can span
  multiple lines).
- Line endings are unified: `\r\n`, `\r`, and `\n` all terminate a record.
- `String::ptr()` is UTF-32 code points, **not** UTF-8 bytes — iterate with
  `length()` / `operator[]`.
- A trailing newline is fine; a final record without one is fine too.
- Strict mode: a row with a different field count than the first row is a hard
  error. Lenient mode (default): short rows are padded with `""`, long rows are
  truncated, and a warning is recorded.

## Two-pass column typing

Pass 1 classifies every non-empty cell (int / float / bool / vector / Color /
array-of-X / JSON / string); per-column unification collapses `INT+FLOAT → FLOAT`,
any `STRING` wins, mixed arrays → `string[]`. Pass 2 converts with
`String → Variant` against the unified type. With a `row_type` present, the
target types come from the row object's declared properties instead; the
inferred `column_types` are baked into the imported `.tres` for inspection and
overrides.

The two-pass design avoids the O(n²) retroactive rewrites that the GDScript CSV
plugins performed (they re-scanned earlier rows whenever a later row disagreed
with the running type guess).

## Reflection binding

`VCSVDataTable.ensure_loaded()`:
1. Instantiates the row type (`Script::new()` or `ClassDB::instantiate`).
2. Reads `Object::get_property_list()`, filters by usage
   (`STORAGE | SCRIPT_VARIABLE`, excluding `INTERNAL/GROUP/CATEGORY/SUBGROUP`
   and `Resource`'s own properties: `resource_name`, `resource_path`,
   `resource_local_to_scene`, `script`).
3. Maps header columns → property names (case-insensitive option).
4. For each row, converts every cell against the property's `PropertyInfo`
   (or a `column_types` override, or a user `Callable` converter) and
   `Object::set()`s it. Failing cells keep the default and are recorded.

Only the string grid + config is serialized; row objects are always rebuilt
from the *current* `row_type` script, so hot-reload re-binds automatically.

### Property filtering details

The reflection binder only binds properties that are:
- `PROPERTY_USAGE_STORAGE` **or** `PROPERTY_USAGE_SCRIPT_VARIABLE`, and
- not `INTERNAL` / `GROUP` / `CATEGORY` / `SUBGROUP`.

This keeps `Resource`'s own bookkeeping (`resource_name`, `resource_path`,
`resource_local_to_scene`, `script`) out of the row mapping.

## Two v0.2.x runtime modes

- **Lazy build** (`lazy_build = true`) — `ensure_loaded()` builds only the
  structure (layout + key index); typed rows are constructed on demand by
  `build_row()` / `get_row()` / `get_row_by_index()`, each result cached.
  Default `false` keeps the eager path (build every row up front).
- **Hot reload** — a table with `source_path` + `hot_reload` set registers in a
  static registry (`VCSVDataTable.get_hot_tables()`); `poll_hot_reload()`
  re-parses the source CSV when its mtime changes and marks the cache dirty. The
  editor plugin calls `poll_hot_reload()` on all registered tables after a
  filesystem scan. Tables created via `from_file` / the editor import default to
  `vortariscsv/general/lazy_build_default` / `hot_reload_default`.

### The typed-row cache

`VCSVDataTable` keeps:
- `headers_` + `rows_` (the persisted string grid).
- `key_index_` — a `HashMap<String, int64_t>` for O(1) keyed lookups.
- `cache_` — the built typed rows in row order; `row_to_cache_` maps original
  row indices to compact cache indices.
- `lazy_cache_` — used in lazy mode (index = original row index).

`mark_dirty()` is called by every mutator (`set_rows`, `set_row_type`,
`set_key_column`, `set_column_types`, `sort_rows`, `set_cell_value`, ...), so the
next `get_row()` rebuilds. The key index is O(1) and is rebuilt only when dirty.

## Foreign keys

A property typed `SomeRow` (OBJECT) whose cell holds another table's key is
resolved lazily via `linked_tables`: `resolve_object` finds the linked
`VCSVDataTable` whose row-type class name matches the property's class name and
calls its `get_row(cell)`. Cross-table cycles are the user's responsibility
(a re-entrant build guard leaves unresolved cells null).

`linked_tables` maps a name → resource path (or a `VCSVDataTable` reference):

```gdscript
weapons.set_linked_table("monsters", "res://data/monsters.csv")
# or
weapons.linked_tables = { "monsters": "res://data/monsters.csv" }
```

The `get_related` / `get_related_dict` / `join_rows` helpers build on the same
resolution. Resolved FK objects are cached on the typed row at build time — if
you mutate the linked table, call `refresh()` on the referencing table too.

## Editor plugin wiring

`editor_plugin.gd`:
1. Registers/migrates the hierarchical `vortariscsv/*` project settings (written
   only when absent — user values are never overwritten).
2. Mounts `VCSVEditorImportPlugin` and adds the *Tools* menu item that switches
   existing `.csv` files to the Vortaris importer.
3. Connects `filesystem_changed` to poll hot-reload tables.
4. Registers the **CSV** main screen (`csv_main_screen.gd`) and wires the
   FileSystem-dock selection logic (single-click selects, double-click opens in
   the CSV tab; `vortariscsv/editor/auto_switch_to_csv` gates the tab switch).

The C++ import plugin's `_get_priority()` is dynamic: `2.0` when
`vortariscsv/import/override_translation_importer` is true (default), else `0.5`
so Godot's built-in translation CSV importer is default again. Per-asset
switching stays available via the Import dock's *Import As* dropdown.

## Build architecture

`SConstruct`:
- Locates godot-cpp via `godot_cpp_path=` / `GODOT_CPP_PATH` / common relative
  paths.
- Globs `src/core/`, `src/reflect/`, `src/gdscript/`, `src/editor/`,
  `src/demo/` + `src/register_types.cpp`.
- For `editor`/`template_debug` targets, compiles `doc_classes/*.xml` into
  `src/gen/doc_data.gen.cpp` via `GodotCPPDocData`, so the in-editor class
  reference (`F1`) reflects the XML. Release builds skip it.
- Strips the Unix `lib` prefix (`SHLIBPREFIX = ""`) and normalizes `linux` →
  `linuxbsd` to match the `.gdextension` keys.

## Performance notes

- Single-pass parse, O(n); per-field allocation, never per-char.
- Two-pass typing avoids the O(n²) retroactive rewrites in the GDScript plugins.
- `VCSVDataTable` builds rows once and caches; lookups are O(1) via the key map.
- `lazy_build = true` defers typed-row construction (structure-only build), so
  loading a huge file's rows costs only what you touch.
- `load_typed` / `load_csv_typed` (v0.3.1) bind the row type once and cache the
  table, so hot-path `get_row()` calls hit the typed-row cache instead of
  rebuilding a table per lookup.
- The pure C++ core returns raw `PackedStringArray` grids; `Variant` is only
  touched in the typed layers, off hot paths.
