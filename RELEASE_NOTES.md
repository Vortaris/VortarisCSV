# VortarisCSV Release Notes

## v0.3.0 (2026-08-16)

The CSV editor moves out of the right dock and becomes a **main-screen workspace**
(the "CSV" tab, next to 2D/3D/Script/AssetLib), mirroring the VortarisModLoader
main-screen pattern: toolbar + split view + clean separators + status bar.

### Editor: CSV main screen

- **Main-screen tab** — the plugin now registers `_has_main_screen()` /
  `_make_visible()` / `_get_plugin_name()` ("CSV") / `_get_plugin_icon()`. The old
  right-dock preview (`editor_table_preview.gd`) and its *Tools* toggle were
  removed — the main screen supersedes them.
- **Open from the FileSystem dock** — activating a `.csv` that the Vortaris
  importer owns (double-clicking it) switches to the CSV tab and opens it in the
  editor. Translation-imported `.csv` files keep Godot's default editor.
- **Editable, resizable data table** — headers are always visible (fixes the
  "first column header not shown" bug: `column_titles_visible` was never enabled
  and the hidden root row rendered a spurious `" "` first row). Columns are
  drag-resizable via a new `VCSVResizableTree` (adapted from
  `VMLResizableTree`). Double-click a cell to edit; the change is written back to
  the source `.csv` with `VCSVWriter` and reimported on the next `process_frame`
  (one-shot, re-entrancy guarded — the 0.2.1 fix is preserved).
- **Import / Export** — toolbar *Import CSV* (pick any file), *Export CSV*
  (serialize the current table via `VCSVWriter`), and *Export Rows* (write only
  the selected rows).
- **Details & validation panel** — shows row/column count, headers, inferred
  column types (`VCSVUtil.detect_types`), and validation issues
  (`VCSVDataTable.validate()` plus row-width / empty-header / duplicate-header
  checks). A bottom status bar reports the last operation.
- **Layout** — toolbar, `HSeparator`, `HSplitContainer` (table | `VSeparator` |
  details), bottom status bar with `HSeparator`.

### Project settings

`vortariscsv/*` settings are reorganized into a hierarchical layout so they group
cleanly in *Project → Project Settings*, and several new options are added:

| Section | Setting | Default | Effect |
|---|---|---|---|
| `general` | `verbose` | `false` | Gated verbose logging (migrated from the flat `vortariscsv/verbose`; the old value is copied to this path on editor startup and the flat key is removed so only one `verbose` shows in Project Settings — C++ readers still fall back to the flat path for projects not yet opened in the editor) |
| `general` | `lazy_build_default` | `false` | New tables from `VCSVDataTable.from_file()` / the editor import default to lazy build |
| `general` | `hot_reload_default` | `false` | New tables default to hot reload |
| `import` | `override_translation_importer` | `true` | (existing) importer-priority override |
| `import` | `delimiter` | `,` | Default delimiter in the Import dock |
| `import` | `encoding` | `utf8` | Default file encoding in the Import dock |
| `import` | `auto_detect_delimiter` | `false` | Default auto-detect switch in the Import dock |
| `import` | `header_rows` | `1` | Default header-row count in the Import dock |
| `editor` | `table_font_size` | `14` | CSV main-screen data-table font size |
| `validation` | `check_duplicate_keys` | `true` | `validate()` runs the duplicate-key check by default |
| `validation` | `check_required_columns` | `true` | `validate()` runs the required-column check by default |

- Settings are registered by `editor_plugin.gd`, written only when absent (user
  values are never overwritten — the ML F4 fix pattern); `ProjectSettings.save()`
  fires once when anything was written.
- Every reader uses the new path with a fallback to the old path (e.g. the verbose
  toggle), so existing projects keep working unchanged.
- **Flat-path cleanup** — when the old flat `vortariscsv/verbose` exists, its value
  is migrated to `vortariscsv/general/verbose` and the flat key is erased, so the
  Project Settings dialog no longer shows two `verbose` entries. `hint_string` is
  never used for free-text descriptions (Godot 4.7 parses it for enum/range/array
  hints; free text there breaks the editor — see the VortarisModLoader "Cannot get
  class" bug). `add_property_info()` has no `description`/tooltip key in 4.7, so
  per-setting descriptions live in this table and the README reference.
- New `demo/scripts/test_settings.gd` headless suite covers default reads,
  `from_file()` lazy/hot defaults, and `validate()` validation defaults (an
  explicit option key wins over the project setting).

### Misc

- `plugin.cfg` version → `0.3.0`.
- New `demo/scripts/test_editor_gui.gd` headless smoke covering parse/populate,
  details, cell-edit write-back (with the reimport guard), and export.

---

## v0.2.1 (2026-08-15)

Patch release: fixes the editor preview save errors, makes the preview panel
opt-in, adds a headless CLI for AI/CI debugging, gated logging, and AI-facing docs.

### Fixes

- **Editor preview save no longer floods the console with errors** — the
  `item_edited` handler no longer calls `reimport_files()` synchronously (that
  printed a stack of `editor/gui/progress_dialog.cpp` "Do not use progress dialog
  while flushing the message queue" errors). The reimport is deferred to the next
  `process_frame` (one-shot) with a re-entrancy guard that coalesces rapid edits.
- **Preview panel is hidden by default** — it no longer auto-docks on plugin
  load. A *Project → Tools* "VortarisCSV: 显示/隐藏 CSV 预览" toggle shows/hides it.

### CLI

- **`demo/scripts/cli_entry.gd`** — headless `--script` entry for AI / CI:
  `--vortaris-csv-validate <file>` (exit 0 = clean, 1 = issues/failure) and
  `--vortaris-csv-stats <file>` (headers, inferred column types, per-column
  stats). All output is `[vortariscsv]`-prefixed for easy parsing.

### Logging

- **Gated logging** — `log_info` prints only in debug builds (compiled out in
  `template_release`); `log_verbose` additionally requires the new
  `vortariscsv/verbose` Project Setting (`true`). Applied to the parser, editor
  import, `VCSVDataTable.rebuild`, hot reload `poll_hot_reload` and `validate`.
  Errors/warnings stay ungated.

### Docs

- **`docs/AI_DEBUGGING.md`** — MCP `run_script` API snippets,
  `VCSVUtil.load_csv_dict_array` / `VCSVDataTable.from_file().validate()` /
  `VCSVParseOptions` usage, the CLI parameter table with exit codes, and the
  log-level matrix. Linked from `README.md` and `README.zh-CN.md`.

---

## v0.2.0 (2026-08-15)

The "ergonomics" release: array cells are forgiving, the importer is the default,
big tables load lazily, and the editor gets a live CSV preview.

### Fixes

- **Array-typed properties accept native Arrays and mixed forms** — JSON array
  literals (`[1,2,3]`), `;`-separated strings, and a mix of both in one column now
  bind to `Array[int]` / `Array[String]` properties correctly. The old code called
  `set_typed()` on a non-empty array (a hard error); it now rebuilds a typed array
  element-by-element. User converters returning native Arrays are coerced to the
  declared element type too.
- **Editor import errors carry line/column** — `push_error` now appends
  `(line N, col M)`.
- **Extra CSV columns are surfaced** — columns with no matching row-type property
  produce a warning naming the column.
- **Invalid header schema types fall back to string** — `hp:notatype` strips the
  annotation and treats the column as `string` instead of leaking a bogus type.
- `get_type_by_name`'s `VARIANT_MAX` sentinel is rejected when validating schema
  type names.

### Improvements

- **`load_csv_dict(path, options)`** — single-row CSVAccess-style loader.
- **Custom import delimiter** — the editor import panel adds a `Custom` delimiter
  with a single-character field.
- **VortarisCSV is the default `.csv` importer** — enabling the plugin writes
  `vortariscsv/import/override_translation_importer=true` when unset, and a
  *Project → Tools* menu item one-click switches existing `.csv` files from the
  built-in translation importer and reimports them.
- **Explicit schema / header type columns** — `VCSVParseOptions.header_type_separator`
  (default off) turns `hp:int` headers into clean `hp` headers with an explicit
  `int` type (also on `VCSVParseResult.column_types`); the editor import panel
  gains a `column_types` text field (`hp:int;attack:float`) that wins over detection.
- **Incremental / delta export** — `export_rows_to_csv(keys, path)` and
  `export_row_to_csv(key, path)` write a subset via `VCSVWriter`.
- **Hot reload** — `source_path` / `hot_reload` / `hot_reload_interval` +
  `poll_hot_reload()` re-parse a source CSV when its mtime changes; the editor
  plugin polls registered tables on filesystem changes.
- **Large-data lazy loading** — `row_offset` / `max_rows` slice a parse, and
  `lazy_build=true` builds only the structure up front, constructing typed rows
  on demand (default `false` keeps the eager path byte-for-byte compatible).
- **Multi-delimiter / multi-header** — `auto_detect_delimiter` picks the most
  consistent quote-aware delimiter from `delimiter_candidates`; `header_rows` /
  `header_join` merge multi-level header rows.

### New Features

- **`validate(options)`** — data-integrity checks (missing required columns,
  type-conversion failures, duplicate keys, unresolved foreign keys).
- **`get_table()`** — CSVAccess-style alias for `to_table()`.
- **Editor table preview panel** — a right-bottom dock renders the selected
  `.csv` as a grid; double-click a cell to edit and write back to the source CSV,
  then reimport.

### Verification

- Full headless suite green across 10 scripts (parser / writer / types /
  data-table script / data-table C++ / import / aux / features / validation /
  perf), plus `--editor --import --quit`.

### Downloads

`vortariscsv-0.2.0-windows.zip` — ready-to-use plugin (Windows, debug + release
DLLs). Build Linux/macOS per `docs/cross_platform.md`.

---

## v0.1.2 (2026-08-11)

Code-review hardening release (independent audit + fixes).

### Fixes

- **Build-failure no longer hides**: `ensure_loaded()` returns `false` consistently
  after a structural build failure (invalid row type), so downstream calls stop
  silently returning empty data.
- **Unresolved foreign keys are surfaced**: an OBJECT cell that cannot resolve
  (no linked table / wrong row-type class / missing key) is recorded in
  `get_last_errors()` instead of failing silently to `null`.
- **`sort` preserves rows**: non-`PackedStringArray` entries are kept (as empty
  rows) instead of being silently dropped.
- **`get_row` index alignment**: rows are looked up through an explicit
  original→cache mapping, so mixed row types no longer return `null` for valid keys.
- **Full GBK table**: decoding now covers the entire GBK range (GB2312 core +
  extensions), so rare characters decode correctly; the `"gbk"` option is honest.
- **Import type inference no longer overrides declared property types** when a
  `row_type` is set.
- **Performance**: the key index is O(1) on lookups (was rebuilt per call); the
  row script is loaded once per build instead of once per row.
- **Parsing** — hex colors with `alpha==0` parse correctly; nested literals like
  `AABB(Vector3(...), Vector3(...))` parse; JSON array literals match the
  declared element type; column lookups respect `case_insensitive_columns`;
  GBK input strips a stray UTF-8 BOM; `to_json_string` keeps full float
  precision; parse warnings surface through `from_file`.
- **Docs**: `doc_classes` const qualifiers match the C++ methods; stale-FK-refresh
  note added to `docs/data_types.md`.

### Verification

- 270 checks across 8 headless suites (added regression tests for each fix).

### Downloads

`vortariscsv-0.1.2-windows.zip` — ready-to-use plugin (Windows, debug + release DLLs).
Build Linux/macOS per `docs/cross_platform.md`.

---

## v0.1.1 (2026-08-11)

Adds encoding support, aggregations, batch APIs and cross-table joins, plus
packaged plugin archives and bilingual README links.

### New features

- **GBK / GB2312 encoding** — `VCSVParseOptions.encoding` (`utf8` default,
  `gbk`, `gb2312`) and a matching editor-import option decode legacy
  Chinese-encoded CSV files (GB2312 core + common GBK range).
- **Aggregations** — `column_stats(column)` on `VCSVTable` / `VCSVDataTable`
  returns `{count, non_empty, numeric, min, max, sum, avg, distinct}`.
- **Batch APIs** — `add_rows`, `append_dicts`, `set_row_dict` for editing many
  cells/rows in one call.
- **Cross-table queries** — `get_related`, `get_related_dict` and `join_rows`
  build flattened dictionaries across `linked_tables` foreign keys.
- **README language switch** — `README.md` ⇄ `README.zh-CN.md` link each other.

### Fixes

- `row_type_class_name` now correctly strips the script extension, so
  foreign-key matching works when the file name and `class_name` differ
  (e.g. `monster_row.gd` / `MonsterRow`).

### Verification

- 260 checks across 8 headless suites (parser / writer / types / GDScript rows /
  C++ rows / import / aux / features).
- Debug + release DLLs build on Windows; code is platform-neutral
  (Windows/Linux/macOS entries in the `.gdextension`).

### Downloads

The plugin archive (`addons/vortariscsv/`) is attached to this release for
Windows; build the Linux/macOS binaries per `docs/cross_platform.md` and drop
them into `addons/vortariscsv/bin/`.

---

## v0.1.0 (2026-08-11)

Initial release.

- **Low-level** — RFC 4180 parser (`VCSVParser` / `VCSVTable` / `VCSVWriter`):
  quoted fields, escaped quotes, multi-line fields, CRLF/LF/CR, BOM, comments,
  configurable delimiter/quote, strict vs lenient rows, line/column errors.
- **High-level** — `VCSVDataTable` (UE-DataTable style): reflection binding to
  GDScript (`extends Resource`) or C++ row types, lazy typed cache, keyed
  lookups, foreign keys, `VCSVUtil` one-liners.
- **Editor** — import plugin turns `.csv`/`.tsv` into `VCSVDataTable` `.tres`
  resources; three-tier control over the built-in translation importer.
- **Auxiliary** — sort (numeric-aware), find (6 match modes), filter, distinct,
  column access, grid editing, JSON/dictionary interop.
- Bilingual docs (`doc_classes` compiled into the DLL), guides in `docs/`,
  MIT license, target Godot 4.7+.
