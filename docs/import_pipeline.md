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

## Import options

| Option | Default | Meaning |
|---|---|---|
| `delimiter` | Comma | Field separator (Comma/Tab/Semicolon/Space/Custom) |
| `delimiter_custom` | `,` | Single-character delimiter, used only when `delimiter` is Custom |
| `auto_detect_delimiter` | `false` | Auto-detect the delimiter from the first ~8 records |
| `delimiter_candidates` | `,;\t\|` | Candidate delimiters for auto-detect |
| `header_rows` | `1` | Number of header rows (>1 = multi-level, joined with `header_join`) |
| `header_join` | `.` | Separator for joining multi-level header rows |
| `quote` | `"` | Quote character |
| `has_header` | `true` | First row is the header |
| `key_column` | (first column) | Lookup-key column for `get_row` |
| `row_type` | empty | `res://...gd` script path for typed rows |
| `detect_types` | `true` | Bake inferred `column_types` into the `.tres` |
| `column_types` | empty | Explicit types as `hp:int;attack:float`; wins over `detect_types` |
| `trim_whitespace` | `true` | Trim unquoted fields |
| `skip_blank_lines` | `true` | Skip blank records |
| `comment_prefix` | empty | Skip lines starting with this character |
| `array_delimiter` | `;` | Cell sub-delimiter for array-typed properties |
| `null_token` | empty | Cells equal to this keep the property default |
| `case_insensitive_columns` | `false` | Match columns to properties ignoring case |

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

## Editor main screen & hot reload (v0.3.x)

- **CSV main screen** — the editor plugin registers the **CSV** tab (next to
  2D/3D/Script) with a full table editor: `demo/addons/vortariscsv/csv_main_screen.gd`
  renders the selected `.csv` as a grid with drag-resizable columns, a details
  panel (rows / cols / headers / inferred types / validation), and Import CSV /
  Export CSV / Export Rows toolbar actions. Double-click a cell to edit it and
  write the change back to the source CSV, then reimport. Activating a
  Vortaris-imported `.csv` in the FileSystem dock (double-click) opens it in the
  CSV tab. (The old right-dock preview and its *Tools* toggle were removed in
  v0.3.0.)
- **Hot reload** — give an imported `VCSVDataTable` a `source_path` and set
  `hot_reload = true`; the editor plugin polls registered tables on filesystem
  changes (`VCSVDataTable.poll_hot_reload()`), re-parsing the source CSV when its
  mtime changes and marking the typed-row cache dirty.

## Runtime-only alternative

Prefer no editor dependency? `VCSVDataTable.from_file()` parses at runtime:

```gdscript
var table := VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
```
