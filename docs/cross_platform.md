# Cross-platform build

The plugin is pure C++/godot-cpp, so it builds on every platform Godot 4.7
supports. The demo `.gdextension` lists all four entries; build each on its own
platform and drop the artifact into `demo/addons/vortariscsv/bin/`.

The extension's `.gdextension` uses `compatibility_minimum = "4.7"` and is
forward-compatible with newer Godot 4.x releases.

## Prerequisites

- [SCons](https://scons.org/) (`pip install scons`).
- A C++ compiler: MSVC (Windows), GCC/Clang (Linux), Xcode Clang (macOS).
- A [godot-cpp](https://github.com/godotengine/godot-cpp) checkout matching your
  Godot version (v10 `master` bundles Godot 4.7's `extension_api.json`).

### Pre-build godot-cpp once per platform/target

```sh
# inside the godot-cpp checkout
scons platform=windows target=template_debug arch=x86_64
scons platform=windows target=template_release arch=x86_64
```

The plugin build then reuses that static library via `build_library=False`. If
you don't pass `build_library=False`, the plugin build tries to build godot-cpp
again — keep it off to save time.

## Windows (MSVC)

```sh
scons platform=windows target=template_debug arch=x86_64 \
      godot_cpp_path=<path-to-godot-cpp> build_library=False
# → demo/addons/vortariscsv/bin/vortariscsv.windows.template_debug.x86_64.dll
scons platform=windows target=template_release arch=x86_64 \
      godot_cpp_path=<path-to-godot-cpp> build_library=False
```

## Linux

```sh
scons platform=linuxbsd target=template_debug arch=x86_64 godot_cpp_path=... build_library=False
scons platform=linuxbsd target=template_release arch=x86_64 godot_cpp_path=... build_library=False
```

> The SConstruct normalizes `platform=linux` → `linuxbsd` so the artifact name
> matches the `.gdextension` key. You can pass either.

## macOS

```sh
scons platform=macos target=template_debug arch=universal godot_cpp_path=... build_library=False
scons platform=macos target=template_release arch=universal godot_cpp_path=... build_library=False
```

## Mobile / web

`platform=android` / `ios` / `web` work the same way; the demo `.gdextension`
does not list them, so add the corresponding `[libraries]` entries when you
ship there:

```ini
android.debug.arm64 = "res://addons/vortariscsv/bin/vortariscsv.android.template_debug.arm64.so"
android.release.arm64 = "res://addons/vortariscsv/bin/vortariscsv.android.template_release.arm64.so"
ios.debug = "res://addons/vortariscsv/bin/vortariscsv.ios.template_debug.universal.dylib"
ios.release = "res://addons/vortariscsv/bin/vortariscsv.ios.template_release.universal.dylib"
web.debug.wasm32 = "res://addons/vortariscsv/bin/vortariscsv.web.template_debug.wasm32.wasm"
web.release.wasm32 = "res://addons/vortariscsv/bin/vortariscsv.web.template_release.wasm32.wasm"
```

## The `.gdextension` file

`demo/addons/vortariscsv/vortariscsv.gdextension`:

```ini
[configuration]
entry_symbol = "vortariscsv_library_init"
compatibility_minimum = "4.7"

[libraries]
windows.debug.x86_64 = "res://addons/vortariscsv/bin/vortariscsv.windows.template_debug.x86_64.dll"
windows.release.x86_64 = "res://addons/vortariscsv/bin/vortariscsv.windows.template_release.x86_64.dll"
linuxbsd.debug.x86_64 = "res://addons/vortariscsv/bin/vortariscsv.linuxbsd.template_debug.x86_64.so"
linuxbsd.release.x86_64 = "res://addons/vortariscsv/bin/vortariscsv.linuxbsd.template_release.x86_64.so"
macos.debug.universal = "res://addons/vortariscsv/bin/vortariscsv.macos.template_debug.universal.dylib"
macos.release.universal = "res://addons/vortariscsv/bin/vortariscsv.macos.template_release.universal.dylib"
```

Artifact naming follows `vortariscsv.<platform>.<target>.<arch><suffix>`, with
the `lib` prefix stripped on Unix (the `.gdextension` references unprefixed
names).

## godot-cpp

Use a checkout matching your Godot version. godot-cpp `master` (v10) bundles a
Godot 4.7 `extension_api.json` and supports 4.3+ via its `api_version`
parameter; a `4.x` stable branch also works if it covers your engine version.
Pre-build its static library once per platform/target:

```sh
scons platform=windows target=template_debug arch=x86_64   # inside godot-cpp
```

`build_library=False` tells the plugin build to reuse that prebuilt library.

## Verifying a build

After building, run the smoke test:

```sh
godot --headless --path demo --quit
# expects "VortarisCSV demo loaded" and exit 0
```

and the regression suite (one script per layer):

```sh
godot --headless --path demo --script res://scripts/test_parser.gd
# ... every test_*.gd exits 0 on success
```

> **Fresh clone?** `.godot/extension_list.cfg` is gitignored and doesn't exist
> until Godot scans the project once. Run
> `godot --headless --editor --import --quit --path demo` first (or open the
> project in the editor) so `--script` mode loads the extension.
