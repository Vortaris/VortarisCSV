# VortarisCSV Release Notes

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
