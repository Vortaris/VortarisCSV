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
- `compatibility_minimum = "4.7"` (GDExtension is forward-compatible)

```gdscript
# One-liner: read a CSV as Array[Dictionary] (CSVAccess-style)
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
var first: Dictionary = VCSVUtil.load_csv_dict("res://data/monsters.csv")  # single-row variant

# UE-DataTable-style: bind rows to a typed GDScript class
var table: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
var goblin: MonsterRow = table.get_row("goblin")
print(goblin.health, " ", goblin.position)
```

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
- **Project settings** — `vortariscsv/*` settings are grouped under four sections, so each one shows up
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
"<godot>/Godot_v4.7-stable_win64_console.exe" --headless --path demo \
      --script res://scripts/test_parser.gd
```

Headless `extends SceneTree` regression tests live in `demo/scripts/` and exit with `0`/`1`.

## Documentation

- `docs/quickstart.md` — 5-minute start
- `docs/data_types.md` — supported cell types, `column_types` syntax, arrays/JSON/foreign keys
- `docs/import_pipeline.md` — editor import, `.tres` storage, priority override
- `docs/architecture.md` — three-layer design, reflection binding, performance
- `docs/AI_DEBUGGING.md` — AI / headless-CLI debugging: MCP `run_script` API snippets, CLI args & exit codes, log levels

## License

MIT — see [LICENSE](LICENSE).
