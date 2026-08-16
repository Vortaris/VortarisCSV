# Editor import pipeline

The add-on ships a C++ `EditorImportPlugin` that imports `.csv` / `.tsv` files
as `VCSVDataTable` resources (`.tres`).

## Enable the plugin

Open the project in the Godot editor and enable **VortarisCSV** under
*Project → Project Settings → Plugins*. The import plugin is registered by
`addons/vortariscsv/editor_plugin.gd`; the runtime itself loads from
`vortariscsv.gdextension` regardless.

When the plugin is enabled, `editor_plugin.gd` also makes VortarisCSV the
**default** `.csv` importer: if the project setting
`vortariscsv/import/override_translation_importer` has not been set yet, it is
written as `true` (so VortarisCSV wins over Godot's built-in translation
importer). A *Project → Tools → "VortarisCSV: .csv -> Vortaris importer"* menu
item switches any existing `.csv` that is still on the translation importer over
to VortarisCSV in one click (and reimports them).

## How a CSV becomes a resource

```
data/monsters.csv ──► VCSVEditorImportPlugin._import()
   ├─ BOM-safe read + VCSVParser (RFC 4180)
   ├─ build VCSVDataTable (headers + string rows + config)
   ├─ infer column_types (when "detect_types" is on)
   └─ ResourceSaver → .godot/imported/monsters.csv-<hash>.tres
```

`load("res://data/monsters.csv")` returns the imported `VCSVDataTable`:

```gdscript
var table: VCSVDataTable = load("res://data/monsters.csv")
var goblin: MonsterRow = table.get_row("goblin")
```

Only the string grid + config is stored in the `.tres` — row objects are
rebuilt from the *current* `row_type` script on first access, so editing the
row script re-binds without re-importing.

### What `_import()` does, step by step

1. Reads the file **as bytes** (not `get_as_text()`) so BOM stripping and
   GBK/GB2312 decoding work at the byte level.
2. Builds a `vortariscsv::CsvParseOptions` from the Import-dock options
   (delimiter enum → single character, quote, header, trim, comments,
   auto-detect, multi-level headers).
3. Decodes with the chosen `encoding` (`utf8` default; `gbk`/`gb2312`), strips a
   UTF-8 BOM, and parses with the RFC 4180 state machine.
4. Splits headers vs data rows (handling `header_rows` > 1 by joining with
   `header_join`), builds a `VCSVDataTable`, and sets `key_column` (first header
   when empty) / `row_type` / `array_delimiter` / `null_token` /
   `case_insensitive_columns`.
5. Bakes `column_types`:
   - An explicit `column_types` text field (`hp:int;attack:float`) **wins**.
   - Otherwise, when there is **no** `row_type` and `detect_types` is on, the
     inferred types are stored. With a `row_type` set, inference is skipped so it
     never fights the row object's declared property types.
6. Saves the `.tres` via `ResourceSaver` with
   `FLAG_REPLACE_SUBRESOURCE_PATHS`.

## Import options

| Option | Default | Meaning |
|---|---|---|
| `delimiter` | Comma | Field separator (Comma/Tab/Semicolon/Space/Custom) |
| `delimiter_custom` | `,` | Single-character delimiter, used only when `delimiter` is Custom (shown conditionally) |
| `auto_detect_delimiter` | `false` | Auto-detect the delimiter from the first ~8 records |
| `delimiter_candidates` | `,;\t\|` | Candidate delimiters for auto-detect |
| `header_rows` | `1` | Number of header rows (>1 = multi-level, joined with `header_join`) |
| `header_join` | `.` | Separator for joining multi-level header rows |
| `encoding` | `utf8` | Text encoding: `utf8`, `gbk`, `gb2312` |
| `quote` | `"` | Quote character |
| `has_header` | `true` | First row is the header |
| `key_column` | (first column) | Lookup-key column for `get_row` |
| `row_type` | empty | `res://...gd` script path (or C++ class name) for typed rows |
| `detect_types` | `true` | Bake inferred `column_types` into the `.tres` (only when no `row_type`) |
| `column_types` | empty | Explicit types as `hp:int;attack:float`; wins over `detect_types` |
| `trim_whitespace` | `true` | Trim unquoted fields |
| `skip_blank_lines` | `true` | Skip blank records |
| `comment_prefix` | empty | Skip lines starting with this character |
| `array_delimiter` | `;` | Cell sub-delimiter for array-typed properties |
| `null_token` | empty | Cells equal to this keep the property default |
| `case_insensitive_columns` | `false` | Match columns to properties ignoring case |

### Project-setting defaults

The **delimiter**, **encoding**, **auto_detect_delimiter** and **header_rows**
defaults shown in the Import dock come from the `vortariscsv/import/*` project
settings (`delimiter` default `,`, `encoding` default `utf8`,
`auto_detect_delimiter` default `false`, `header_rows` default `1`). Per-asset
values chosen in the Import dock are stored in the `.import` file and still win
over the project defaults. Newly imported tables also default to the
`vortariscsv/general/lazy_build_default` and `vortariscsv/general/hot_reload_default`
settings (like `VCSVDataTable.from_file()`), and record the source `.csv` in
`source_path` so hot reload has a file to re-parse.

## Priority vs. the built-in translation importer

Godot's built-in importer handles `.csv` as *translations*. VortarisCSV claims
`.csv`/`.tsv` with priority `2.0` by default so data tables win. Three ways to
control it:

1. **Project setting** `vortariscsv/import/override_translation_importer`
   (default `true`). Set it to `false` and the importer drops to priority `0.5`,
   letting the translation importer win by default again.
2. **Per asset**: with the plugin enabled, select any `.csv` in the FileSystem
   dock and use the Import dock's *Import As* dropdown to switch between
   "Translation" and "Vortaris CSV Data".
3. Reimport on change: modify a file (or its import options) and Godot
   re-imports automatically.

> The priority switch is **dynamic** — `_get_priority()` re-reads the project
> setting on every call, so flipping `vortariscsv/import/override_translation_importer`
> takes effect immediately (no plugin reload needed). Per-asset choices made in
> the Import dock are stored in the `.import` file and always win.

## Editor main screen & hot reload (v0.3.x)

- **CSV main screen** — the editor plugin registers the **CSV** tab (next to
  2D/3D/Script) with a full table editor: `demo/addons/vortariscsv/csv_main_screen.gd`
  renders the selected `.csv` as a grid with drag-resizable columns, a details
  panel (rows / cols / headers / inferred types / validation), and Import CSV /
  Export CSV / Export Rows toolbar actions. Double-click a cell to edit it and
  write the change back to the source CSV, then reimport. Double-clicking a
  Vortaris-imported `.csv` in the FileSystem dock opens it in the CSV tab — a
  single click only selects and never switches you out of your current editor.
  Set `vortariscsv/editor/auto_switch_to_csv` to `false` to keep even
  double-click from switching tabs. (The old right-dock preview and its *Tools*
  toggle were removed in v0.3.0.)
- **Hot reload** — an imported `VCSVDataTable` records its source `.csv` in
  `source_path` (so `poll_hot_reload()` has a file to re-parse) and, when
  `vortariscsv/general/hot_reload_default` is on (or the per-asset `hot_reload`
  is set), registers for polling. The editor plugin polls registered tables on
  filesystem changes (`VCSVDataTable.poll_hot_reload()`), re-parsing the source
  CSV when its mtime changes and marking the typed-row cache dirty.

## Multi-level headers

A CSV with repeated group headers can be merged into one flat header row:

```csv
Level,Level,Name
Health,Attack,-
100,10,goblin
```

With `header_rows = 2` and `header_join = "."`, the imported headers become
`["Level.Health", "Level.Attack", "Name.-"]`. The joining is applied at parse
time (`vortariscsv::join_header_rows`) and works for any number of header rows.

## Troubleshooting imports

- **The `.csv` is imported as a Translation, not a data table** — either the
  plugin isn't enabled, `vortariscsv/import/override_translation_importer` is
  `false`, or the file was already imported by the translation importer. Use the
  *Tools → "VortarisCSV: .csv -> Vortaris importer"* menu item, or switch the
  file in the Import dock's *Import As* dropdown.
- **Import fails with "line N, col M"** — the file has a parse error (e.g.
  unterminated quote, uneven row in strict mode, multi-character delimiter).
  Fix the CSV and reimport.
- **`get_row()` returns `null`** — the `.tres` has no `row_type`, or the
  `key_column` value doesn't match. Check the Import-dock *Row Type* and
  *Key Column* options.
- **The table doesn't reflect your CSV edits** — the editor reimports on change;
  if it seems stale, force a reimport (select the file, Import dock, *Reimport*),
  or enable hot reload.

## Runtime-only alternative

Prefer no editor dependency? `VCSVDataTable.from_file()` parses at runtime:

```gdscript
var table := VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
```

This is exactly what the import plugin does internally, minus the `.tres` save.
Use it for procedurally generated data, `user://` files, or assets you don't want
in the editor's import cache.
