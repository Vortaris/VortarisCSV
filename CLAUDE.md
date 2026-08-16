# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & verify (Windows / MSVC / SCons)

The plugin links godot-cpp (external dependency; use v10 `master`, which bundles
Godot 4.7's `extension_api.json`, or a branch matching your Godot version).
Build its static library first: `scons platform=windows target=template_debug arch=x86_64`.
Point the plugin build at your checkout with `godot_cpp_path=` (or the
`GODOT_CPP_PATH` env var). `godot` below is your Godot 4.7 console binary.

```bash
# Build the plugin DLL (outputs to demo/addons/vortariscsv/bin/)
scons -j 8 platform=windows target=template_debug arch=x86_64 build_library=False \
      godot_cpp_path=<path-to-godot-cpp>

# Functional smoke (expect "VortarisCSV demo loaded", exit 0)
godot --headless --path demo --quit

# Regression suite (one script per layer; exit 0 = all pass)
godot --headless --path demo --script res://scripts/test_parser.gd
godot --headless --path demo --script res://scripts/test_writer.gd
godot --headless --path demo --script res://scripts/test_types.gd
godot --headless --path demo --script res://scripts/test_datatable_script.gd
godot --headless --path demo --script res://scripts/test_datatable_cpp.gd
godot --headless --path demo --script res://scripts/test_import.gd
godot --headless --path demo --script res://scripts/test_aux.gd
godot --headless --path demo --script res://scripts/test_features.gd
godot --headless --path demo --script res://scripts/test_validation.gd

# Performance smoke (soft timing targets; exit 0 = OK)
godot --headless --path demo --script res://scripts/perf_test.gd
```

Rules of thumb when changing code: **every structural/behavioral change must keep
the demo AND the relevant regression suites green**; run them together. A fresh
demo checkout needs `.godot/extension_list.cfg` (open the project in the editor
once, or run any `--headless --path demo` command once).

`doc_classes/*.xml` is compiled into the DLL for `editor` and `template_debug`
builds (see SConstruct `GodotCPPDocData`), so the in-editor class reference (`F1`)
reflects the XML. **After editing any doc XML, rebuild the DLL and re-run
`godot --headless --editor --import --quit --path demo`** so the class docs in
`res://.godot/extension_list.cfg` / the imported cache are refreshed.

## Architecture

Three layers, by design — the pure C++ core never allocates per character and
never touches Variant on hot paths:

- **`src/core/` (`namespace vortariscsv`)** — the algorithms: `csv_parser`
  (RFC 4180 state machine), `csv_writer` (quoting / line endings / injection
  guard), `type_inference` (two-pass column typing), `type_converter`
  (`String → Variant`), `column_index` (header → index map).
- **`src/reflect/`** — `reflection_binder` (property-list filtering + column↔
  property mapping + `bind_row`), `row_factory` (`Script::new()` /
  `ClassDB::instantiate()` → `Ref<Resource>`).
- **`src/gdscript/` (`VCSV`-prefixed classes)** — thin GDExtension binding on
  top of the core: `VCSVParser`, `VCSVTable`, `VCSVWriter`, `VCSVDataTable`
  (the high-level DataTable), `VCSVParseOptions`, `VCSVParseResult`, `VCSVUtil`.
- **`src/editor/`** — `VCSVEditorImportPlugin` (`EditorImportPlugin`, registered
  at `MODULE_INITIALIZATION_LEVEL_EDITOR`), wired up by `demo/addons/vortariscsv/editor_plugin.gd`.

### Key invariants (violating these is the classic source of bugs here)

- **Parser is a single-pass `char32_t*` state machine.** Fields accumulate in a
  `std::vector<char32_t>` and are materialized with the `char32_t*` String
  constructor (`string_new_with_utf32_chars`); `String::resize()`+`ptrw()`+`memcpy`
  on a COW String is unreliable, so it is avoided. `String::ptr()` is UTF-32
  code points, NOT UTF-8 bytes — iterate with `length()`/`operator[]` or `ptr()`.
  States: OUTSIDE / IN_QUOTES / AFTER_QUOTES. `""` inside quotes is one literal
  quote; delimiter / CR / LF inside quotes are literal. Unify `\r\n` / `\r` / `\n`.
- **BOM is stripped at the byte level.** Read `FileAccess::get_file_as_bytes`,
  skip `EF BB BF`, then `String::utf8((const char*)pba.ptr(), size)`.
  `get_as_text()` does NOT guarantee BOM stripping.
- **Type inference is two-pass.** Pass 1 classifies each column's non-empty
  cells, then unifies (`INT+FLOAT → FLOAT`, any conflict → `STRING`, element
  mismatch in arrays → `STRING[]`); Pass 2 converts. Never retroactively rewrite
  earlier rows (the GDScript plugins did O(n²) here).
- **`VCSVDataTable` rows are lazily built and cached.** `ensure_loaded()` builds
  once; any change to `row_type` / `key_column` / `column_types` / `rows` marks
  the cache dirty. Row types must extend `Resource`. The `.tres` stores only the
  string grid + config — row objects are always re-built from the *current*
  script, so script hot-reload re-binds automatically.
- **Reflection mapping filters Resource's own properties.** `resource_name`,
  `resource_path`, `resource_local_to_scene`, `script` must be blacklisted;
  include only `PROPERTY_USAGE_STORAGE | SCRIPT_VARIABLE` and exclude
  `INTERNAL | GROUP | CATEGORY | SUBGROUP`.
- **Import-priority override is dynamic.** `_get_priority()` returns `2.0` when
  `vortariscsv/import/override_translation_importer` is true (default), else
  `0.5` so Godot's built-in translation CSV importer is default again. Per-asset
  switching stays available via the Import dock's *Import As* dropdown.

### Testing conventions

Headless `extends SceneTree` scripts in `demo/scripts/`, named `test_<area>.gd`.
Each asserts via `push_error`-style checks and `quit(1)` on failure, `quit(0)` on
success. Keep the RFC 4180 edge cases covered (quotes inside quotes, delimiter /
newline inside quotes, CRLF/LF/CR, trailing newline, empty file, BOM, comment
lines, strict vs lenient row width).
