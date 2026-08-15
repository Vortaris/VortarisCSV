extends SceneTree

# Headless CLI entry point for AI / automation / CI. It loads the same
# GDExtension as the game, so every VCSV* class is available.
#
# One-time prerequisite (fresh clone): the extension cache
# .godot/extension_list.cfg is gitignored, so it does not exist until Godot
# scans the project once. Until then, --script mode skips vortariscsv.gdextension
# and every VCSV* class is missing. Generate the cache with:
#   godot --headless --editor --import --quit --path demo
# (or open the project in the editor once).
#
# IMPORTANT: this file must never reference a VCSV* identifier directly. On a
# fresh clone those classes do not exist, and an unresolved identifier is a hard
# GDScript parse error that aborts before _init() runs — which would mask the
# guard below with a cryptic "Identifier not declared". The actual CLI logic lives
# in cli_impl.gd and is only loaded after the guard confirms the extension exists.
#
# Run:
#   godot --headless --path demo --script res://scripts/cli_entry.gd \
#       -- --vortaris-csv-validate res://data/monsters.csv
#   godot --headless --path demo --script res://scripts/cli_entry.gd \
#       -- --vortaris-csv-stats res://data/monsters.csv
#
# All VortarisCSV arguments must come after `--` (OS.get_cmdline_user_args()).
# Output is prefixed with [vortariscsv] so it is easy to grep / parse.

const IMPL_PATH := "res://scripts/cli_impl.gd"


func _init() -> void:
	# Fresh clones have no .godot/extension_list.cfg (gitignored), so Godot skips
	# loading vortariscsv.gdextension and every VCSV* class is missing. Fail with a
	# clear, actionable error instead of a cryptic "Identifier not declared" parse
	# error (see the one-time prerequisite note at the top of this file).
	if not ClassDB.class_exists("VCSVUtil"):
		print("[vortariscsv] ERROR: GDExtension not loaded (class 'VCSVUtil' not found).")
		print("[vortariscsv]   This is expected on a fresh clone: the extension cache")
		print("[vortariscsv]   (.godot/extension_list.cfg) has not been generated yet, so")
		print("[vortariscsv]   --script mode does not load vortariscsv.gdextension.")
		print("[vortariscsv]   Run this once to generate the cache:")
		print("[vortariscsv]     godot --headless --editor --import --quit --path demo")
		print("[vortariscsv]   (or open the project in the editor once), then re-run this CLI.")
		quit(1)
		return

	var impl = load(IMPL_PATH)
	if impl == null:
		printerr("[vortariscsv] ERROR: failed to load ", IMPL_PATH)
		quit(1)
		return

	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		print(impl.USAGE)
		quit(1)
		return
	match args[0]:
		"--vortaris-csv-validate":
			if args.size() < 2:
				print(impl.USAGE)
				quit(1)
				return
			quit(impl.cmd_validate(args[1]))
		"--vortaris-csv-stats":
			if args.size() < 2:
				print(impl.USAGE)
				quit(1)
				return
			quit(impl.cmd_stats(args[1]))
		_:
			print("[vortariscsv] unknown argument: ", args[0])
			print(impl.USAGE)
			quit(1)
