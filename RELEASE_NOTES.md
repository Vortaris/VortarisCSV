# VortarisCSV Release Notes

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
