# Cross-platform build

The plugin is pure C++/godot-cpp, so it builds on every platform Godot 4.7
supports. The demo `.gdextension` lists all four entries; build each on its own
platform and drop the artifact into `demo/addons/vortariscsv/bin/`.

The extension's `.gdextension` uses `compatibility_minimum = "4.7"` and is
forward-compatible with newer Godot 4.x releases.

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

## macOS

```sh
scons platform=macos target=template_debug arch=universal godot_cpp_path=... build_library=False
scons platform=macos target=template_release arch=universal godot_cpp_path=... build_library=False
```

## Mobile / web

`platform=android` / `ios` / `web` work the same way; the demo `.gdextension`
does not list them, so add the corresponding `[libraries]` entries when you
ship there.

## godot-cpp

Use a checkout matching your Godot version. godot-cpp `master` (v10) bundles a
Godot 4.7 `extension_api.json` and supports 4.3+ via its `api_version`
parameter; a `4.x` stable branch also works if it covers your engine version.
Pre-build its static library once per platform/target:

```sh
scons platform=windows target=template_debug arch=x86_64   # inside godot-cpp
```

`build_library=False` tells the plugin build to reuse that prebuilt library.
