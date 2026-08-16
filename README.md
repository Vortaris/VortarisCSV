# VortarisCSV

**English** | [简体中文](README.zh-CN.md)

> **API reference:** the authoritative, per-class API docs are `doc_classes/*.xml`
> (compiled into the extension so `F1` works in the editor). This README is an
> overview; if it ever disagrees with `doc_classes/`, trust the XML.

Modern CSV/DSV data plugin for **Godot 4.7**, written in **C++ as a GDExtension** (godot-cpp).

Built to be **fast to get started, flexible under the hood**. It combines the best ideas from the popular
GDScript CSV plugins and fixes their weaknesses with a native C++ core:

- RFC 4180-compliant state-machine parser (`char32_t` pointer traversal, single pass, zero per-char allocation)
- **UE-DataTable-style reflection binding**: bind CSV rows directly to custom row types
  (GDScript `extends Resource` classes *or* C++ `ClassDB` classes) — column names map to `@export` properties
- Two-layer API: a fast string-level layer (`VCSVParser` / `VCSVTable` / `VCSVWriter`) and a high-level object
  layer (`VCSVDataTable`), plus a full editor import pipeline (`.csv`/`.tsv` → `.tres`)
- Typed cells: scalars, enums, `Vector2/3/4`, `Color`, `Rect2`, transforms, arrays (sub-delimiter), JSON cells,
  and lazy cross-table references (foreign keys)
- Structured errors with line/column, BOM handling, comments, configurable delimiter/quote, CSV-injection-safe export
- **v0.2.0**: VortarisCSV is the default `.csv` importer (with a one-click switch for existing files); array cells accept both `;`-separated and JSON-array forms; explicit `hp:int` header schemas; `VCSVUtil.load_csv_dict()` single-row loader and `VCSVDataTable.get_table()` alias; hot reload; lazy building for huge files; custom import delimiter; auto-delimiter detection; multi-level headers; `validate()` data-integrity checks; delta export (`export_rows_to_csv` / `export_row_to_csv`); and an editor table preview dock (double-click to edit & write back)
- **v0.2.1**: headless CLI for AI/CI (`res://scripts/cli_entry.gd` → `--vortaris-csv-validate` / `--vortaris-csv-stats`), gated logging (`vortariscsv/verbose`), AI debugging guide (`docs/AI_DEBUGGING.md`), and an editor preview panel that is hidden by default with a manual toggle (save-back reimport errors fixed)
- **v0.3.0**: the CSV editor is now a **main-screen workspace** (the "CSV" tab, next to 2D/3D/Script) — editable + column-drag-resizable data table, cell edit with write-back to the source `.csv`, Import CSV / Export CSV / Export Rows, a details panel (rows/cols/headers/inferred types/validation), and a status bar. Double-clicking a Vortaris-imported `.csv` in the FileSystem dock opens it in the editor (single-click only selects; the tab switch happens on double-click and can be disabled via `vortariscsv/editor/auto_switch_to_csv`). Fixes the missing first-column header. `vortariscsv/*` project settings are reorganized into a hierarchical layout (`general` / `import` / `editor` / `validation`) with new options (see **Project settings** below).
- **v0.3.1**: runtime hot-path ergonomics — `VCSVDataTable.load_typed(path, row_type)` / `VCSVUtil.load_csv_typed(path, options, row_type)` parse a CSV and bind the row type **once** (cache the table and `get_row()` hits the built-in typed-row cache); `VCSVDataTable.get_field_array(key, field)` returns an `Array` column as a native `Array` (no manual `split(";")`); and `Array[String]` columns now round-trip correctly through `load_csv_dict_array` / `load_csv_dict` (they previously came back as raw `;`-joined strings).
- `compatibility_minimum = "4.7"` (GDExtension is forward-compatible)

## Quick start

Three ways to read a CSV, from least to most structure:

```gdscript
# 1) One-liner: read a CSV as Array[Dictionary] (CSVAccess-style).
#    Cell types are inferred per column (int/float/bool/Vector2/Color/Array/...).
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
var goblin: Dictionary = rows[0]
print(goblin.health, " ", goblin.position)     # 100  (10, 20)
var first: Dictionary = VCSVUtil.load_csv_dict("res://data/monsters.csv")  # single-row variant

# 2) Low-level: parse to a raw string grid (fast, no type conversion).
var result := VCSVParser.parse_file("res://data/monsters.csv", null)
if result.success:
    var table: VCSVTable = result.table
    print(table.headers, " ", table.get_row(0))   # ["id",...]  ["goblin", ...]

# 3) UE-DataTable-style: bind rows to a typed GDScript class.
var table: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
var monster: MonsterRow = table.get_row("goblin")
print(monster.health, " ", monster.position)   # 100  (10, 20)

# 4) 0.3.1: typed one-shot — cache the table, then get_row() is a cache hit.
var typed: VCSVDataTable = VCSVDataTable.load_typed(
    "res://data/monsters.csv", "res://scripts/row_types/monster_row.gd")
var tags: Array = typed.get_field_array("goblin", "tags")   # native Array, no ";".split
```

See `docs/quickstart.md` for a guided 5-minute walkthrough and `docs/data_types.md`
for the full cell-type matrix.

## API overview

| Class | Layer | Purpose |
|---|---|---|
| `VCSVParser` | Low-level | Static `parse_string` / `parse_file` → RFC 4180 grid. No type conversion. |
| `VCSVParseOptions` | Low-level | Parser config: delimiter, quote, comments, strictness, BOM, encoding, slicing, auto-detect, multi-level headers, header schema. |
| `VCSVParseResult` | Low-level | Structured outcome: `success`, `error`, `message`, `error_line/column`, `warnings`, `table`, `column_types`. |
| `VCSVTable` | Low-level | String-grid container: cell/row/column access, sort/find, stats, JSON/dict interop, editing. |
| `VCSVWriter` | Low-level | Serialize tables/rows/dicts back to CSV (RFC 4180 quoting, line endings, formula-injection guard). |
| `VCSVDataTable` | High-level | UE-DataTable-style typed table: reflection binding, keyed lookups, lazy build, hot reload, validation, foreign keys, delta export. |
| `VCSVUtil` | High-level | Static one-liners: `load_csv_dict_array`, `load_csv_dict`, `load_csv_typed`, `detect_types`, `table_to_dict_array`, `type_name`. |
| `VCSVEditorImportPlugin` | Editor | C++ `EditorImportPlugin` importing `.csv`/`.tsv` → `.tres` `VCSVDataTable`. |

The general rule of thumb:

- **Read once, inspect quickly** → `VCSVUtil.load_csv_dict_array(path)`.
- **Need a typed object per row, lookups by key, or edits** → `VCSVDataTable.from_file(path, null, row_type)` (or `load_typed` in 0.3.1).
- **Need raw string cells, search/sort on the grid, or custom parsing** → `VCSVParser` + `VCSVTable`.
- **Write CSV** → `VCSVWriter`.

## Highlights

- **Low-level** — `VCSVParser.parse_string()` → `VCSVParseResult` → `VCSVTable`
  (raw string grid, full query API), `VCSVWriter` for serialization.
- **High-level** — `VCSVDataTable`: reflection binding, primary-key lookups, lazy row construction + cache,
  automatic re-binding when the row-type script changes.
- **Editor** — `.csv`/`.tsv` dropped into the project are imported as `VCSVDataTable` resources (`.tres`).
  Configurable per-asset options; overrides Godot's built-in translation CSV importer by default
  (toggle in Project Settings: `vortariscsv/import/override_translation_importer`), and any file can be
  switched back to the translation importer per-asset from the Import dock's *Import As* dropdown.
- **Editor main screen** — the **CSV** tab (next to 2D/3D/Script) hosts a full table editor: draggable
  column widths, double-click cell editing that writes back to the source `.csv`, Import CSV / Export CSV /
  Export Rows, and a details panel (rows / cols / headers / inferred types / validation issues).
  Double-clicking a Vortaris-imported `.csv` in the FileSystem dock opens it in the CSV tab; a single click
  only selects (it never switches you out of your current editor). Set
  `vortariscsv/editor/auto_switch_to_csv` to `false` to keep even double-click from switching tabs.

## Using the editor CSV main screen

1. **Enable the plugin**: open the project in the Godot editor, go to
   *Project → Project Settings → Plugins*, and enable **VortarisCSV**. (The
   runtime itself loads from `vortariscsv.gdextension` regardless of this toggle.)
2. **Drop in a CSV**: put `data/monsters.csv` under `res://`. The Vortaris importer
   takes over `.csv`/`.tsv` automatically (priority 2.0) and imports it as a
   `VCSVDataTable` resource.
3. **Open it in the editor**: double-click the `.csv` in the FileSystem dock. The
   editor switches to the **CSV** tab and renders the table. A single click only
   selects the file and never pulls you out of your current editor.
4. **Edit cells**: double-click any cell, type, and press Enter. The change is
   written back to the source `.csv` (via `VCSVWriter`) and the file is reimported
   on the next frame, so the imported `.tres` stays in sync.
5. **Resize columns**: drag the header separators to your preferred width.
6. **Import / export**: the toolbar has **Import CSV** (open any file),
   **Export CSV** (serialize the whole table), and **Export Rows** (write only the
   selected rows — Ctrl/Cmd-click to multi-select).
7. **Inspect**: the right-hand **Details** panel shows row/column count, headers,
   inferred column types, and any validation issues (row-width mismatches,
   empty/duplicate headers, and `VCSVDataTable.validate()` findings).
8. **Set the row type**: select the `.csv`, open the **Import** dock, set
   *Row Type* to a script like `res://scripts/row_types/monster_row.gd`, and
   reimport. Now `load("res://data/monsters.csv").get_row("goblin")` returns a
   typed `MonsterRow`.

Tips:
- The editor caps the view at 1000 rows for responsiveness; the note row tells you
  the rest is kept and re-saved on edit. The full data is always what's in the file.
- The data-table font size is `vortariscsv/editor/table_font_size` (default 14).
- `vortariscsv/editor/auto_switch_to_csv` (default `true`) controls whether
  double-clicking a Vortaris-imported `.csv` switches to the CSV tab.

## Data-driven integration

The editor-imported `.tres` is just a `VCSVDataTable`, so `load()` works from any
scene or autoload:

```gdscript
# Editor-imported resource: load the .csv path directly (resolves to the .tres).
var table: VCSVDataTable = load("res://data/monsters.csv")
var goblin: MonsterRow = table.get_row("goblin")

# Runtime-only (no editor dependency):
var table := VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
```

Row types are ordinary `extends Resource` classes:

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
@export var notes: Dictionary = {}
```

Only the string grid + config is stored in the `.tres`; typed row objects are
rebuilt from the **current** `row_type` script on first access. That means:

- **Script hot-reload re-binds automatically** — edit `MonsterRow.gd`, save, and
  the next `get_row()` uses the new properties; no reimport needed.
- **Adding a column to the CSV** — just add a `@export` property to the row type,
  or leave it as an extra column (a warning is recorded for columns with no
  matching property).

### Cross-table references (foreign keys)

```gdscript
# WeaponRow has `@export var owner: MonsterRow`.
# weapons.linked_tables = {"monsters": "res://data/monsters.csv"}
var weapons: VCSVDataTable = load("res://data/weapons.csv")
var sword: WeaponRow = weapons.get_row("w1")
print(sword.owner.name)                    # FK resolved lazily to MonsterRow
var joined: Array = weapons.join_rows("monsters")   # flat dicts, "monsters." prefixed
```

> **Refresh note:** typed rows cache their resolved foreign-key objects at build
> time. If you mutate a *linked* table, call `refresh()` on the referencing table
> too so its cached FK references are rebuilt.

### Hot reload

An imported table records its source `.csv` in `source_path`. When
`vortariscsv/general/hot_reload_default` is on (or the per-asset `hot_reload`
option is set), the editor plugin polls registered tables on filesystem changes
and re-parses the source CSV when its mtime changes. At runtime you can drive the
same mechanism manually:

```gdscript
var t := VCSVDataTable.load_typed("user://data.csv", "res://scripts/row_types/monster_row.gd")
t.source_path = "user://data.csv"
t.hot_reload = true
if t.poll_hot_reload():
    print("file changed, cache marked dirty")
```

## Project settings

`vortariscsv/*` settings are grouped under four sections, so each one shows up
under *Project → Project Settings* in its own category. Full reference (path, default, description):

| Setting | Default | Description |
|---|---|---|
| `vortariscsv/general/verbose` | `false` | Gated verbose logging. The 0.2.x flat `vortariscsv/verbose` is migrated to this path on editor startup (the old value is copied, then the flat key is removed so only one `verbose` shows in Project Settings); C++ readers still fall back to the flat path for projects not yet opened in the editor. |
| `vortariscsv/general/lazy_build_default` | `false` | Default for new tables' `lazy_build` (applied by `VCSVDataTable.from_file()` and the editor import). |
| `vortariscsv/general/hot_reload_default` | `false` | Default for new tables' `hot_reload` (re-import the `.tres` when the source `.csv` changes). |
| `vortariscsv/import/override_translation_importer` | `true` | Let the Vortaris importer take over `.csv`/`.tsv` by default (import priority `2.0`); turn off to keep Godot's built-in translation CSV importer as the default. |
| `vortariscsv/import/delimiter` | `,` | Default delimiter used by the editor import (e.g. `,` `;` tab `\|`). |
| `vortariscsv/import/encoding` | `utf8` | Default text encoding used by the editor import (`utf8`, `gbk`, `gb2312`). |
| `vortariscsv/import/auto_detect_delimiter` | `false` | Auto-detect the delimiter on import instead of using the `delimiter` default. |
| `vortariscsv/import/header_rows` | `1` | Number of leading header rows in imported CSVs. |
| `vortariscsv/editor/table_font_size` | `14` | Font size of the CSV main-screen data table. |
| `vortariscsv/editor/auto_switch_to_csv` | `true` | Double-clicking a Vortaris-imported `.csv` in the FileSystem dock switches to the CSV main screen. Set `false` to only open files while the CSV tab is already active. |
| `vortariscsv/validation/check_duplicate_keys` | `true` | Default: `VCSVDataTable.validate()` reports duplicate key-column values. |
| `vortariscsv/validation/check_required_columns` | `true` | Default: `VCSVDataTable.validate()` reports missing `required_columns`. |

Notes:
- Per-asset Import-dock overrides still win over the `vortariscsv/import/*` defaults.
- An explicit option key (`check_duplicate_keys` / `check_required_columns` in the `validate()` options
  dictionary) overrides the `vortariscsv/validation/*` project setting per call.
- **Hover tooltips**: Godot 4.7's `ProjectSettings.add_property_info()` has no `description`/tooltip key —
  descriptions for built-in settings are baked into the editor binary, and custom settings can't supply one.
  The descriptions above are the canonical reference. `hint_string` is only used for its real meaning
  (enum options / range / placeholder text) — never for free-text descriptions, which would break those hints.

## Common questions & gotchas

- **`get_row()` returns `null`** — the key isn't in `key_column`, or `row_type`
  is empty / can't be instantiated. Check `get_last_errors()` for the reason.
- **A cell failed to convert** — the row still builds, the property keeps its
  default, and the failure is recorded in `get_last_errors()` (with row/column).
- **Columns with no matching property** — they're ignored and a warning is
  recorded in `get_last_warnings()`. Properties with no matching column keep their
  default.
- **Multi-character delimiter rejected** — `delimiter` must be a single code point
  (`",", ";", "\t", "|", ...`). A multi-character delimiter is a parse error.
- **GBK files** — set `VCSVParseOptions.encoding = "gbk"` (or `"gb2312"`), or use
  the import-panel `encoding` option. GBK/GB2312 decoding covers the GB2312 core +
  common GBK characters; unknown bytes become U+FFFD.
- **The imported `.tres` is stale** — the plugin re-imports on change; with hot
  reload enabled it re-parses automatically. You can also force a reimport from
  the FileSystem dock.
- **`Array[String]` columns** — from v0.3.1 they round-trip as native `Array`s
  through `load_csv_dict_array` / `load_csv_dict` and `get_field_array(key, field)`;
  you no longer need to `split(";")` by hand.
- **Performance on huge files** — use `VCSVParseOptions.row_offset` / `max_rows`
  to slice a parse, and `lazy_build = true` to defer typed-row construction until
  a row is actually requested.

## Build

Prerequisites: [SCons](https://scons.org/), a C++ compiler (MSVC / MinGW / Clang), and a
[godot-cpp](https://github.com/godotengine/godot-cpp) checkout targeting your Godot version
(v10 `master` supports 4.7 via its bundled `extension_api.json`).

```sh
scons platform=windows target=template_debug arch=x86_64 \
      godot_cpp_path=<path-to-godot-cpp> build_library=False
```

Output: `demo/addons/vortariscsv/bin/vortariscsv.windows.template_debug.x86_64.dll`
(replace `platform`/`target`/`arch` as needed — see `docs/cross_platform.md`).

## Test

```sh
godot --headless --path demo --script res://scripts/test_parser.gd
```

Headless `extends SceneTree` regression tests live in `demo/scripts/` and exit with `0`/`1`:

- `test_parser.gd` — RFC 4180 edge cases (quotes, newlines, line endings, BOM, comments, strictness, auto-detect, multi-header)
- `test_writer.gd` — quoting rules, line endings, always-quote, formula sanitize, round-trips, subset export
- `test_types.gd` — type inference, typed dicts, JSON/Color, header schema
- `test_datatable_script.gd` — GDScript row binding, lookups, errors/warnings, typed arrays, load_typed caching, hot reload
- `test_datatable_cpp.gd` — C++ `ClassDB` row type binding, invalid row type
- `test_import.gd` — import pipeline (parse → `.tres` → reload), custom delimiter
- `test_aux.gd` — sort/find/filter, editing, JSON interop
- `test_features.gd` — GBK, stats, batch APIs, foreign-key joins
- `test_validation.gd` — required columns, conversion errors, duplicate keys, unresolved FKs
- `test_settings.gd` — project-setting defaults and overrides
- `test_editor_gui.gd` — CSV main-screen data path (parse/populate, edit write-back, export)
- `perf_test.gd` — ~1M-cell parse/bind smoke with soft timing targets

Run the CLI entry the same way (see `docs/AI_DEBUGGING.md`):

```sh
godot --headless --path demo --script res://scripts/cli_entry.gd -- \
      --vortaris-csv-validate res://data/monsters.csv
```

## Documentation

- `docs/quickstart.md` — 5-minute start
- `docs/data_types.md` — supported cell types, `column_types` syntax, arrays/JSON/foreign keys
- `docs/import_pipeline.md` — editor import, `.tres` storage, priority override
- `docs/architecture.md` — three-layer design, reflection binding, performance
- `docs/cross_platform.md` — building for Windows/Linux/macOS/mobile/web
- `docs/AI_DEBUGGING.md` — AI / headless-CLI debugging: MCP `run_script` API snippets, CLI args & exit codes, log levels

## License

MIT — see [LICENSE](LICENSE).
