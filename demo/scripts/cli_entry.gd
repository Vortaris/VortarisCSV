extends SceneTree

# Headless CLI entry point for AI / automation / CI. It loads the same
# GDExtension as the game, so every VCSV* class is available.
#
# Run:
#   godot --headless --path demo --script res://scripts/cli_entry.gd \
#       -- --vortaris-csv-validate res://data/monsters.csv
#   godot --headless --path demo --script res://scripts/cli_entry.gd \
#       -- --vortaris-csv-stats res://data/monsters.csv
#
# All VortarisCSV arguments must come after `--` (OS.get_cmdline_user_args()).
# Output is prefixed with [vortariscsv] so it is easy to grep / parse.

const USAGE := "[vortariscsv] usage:\n" \
	+ "  --vortaris-csv-validate <file>   parse the CSV + run data-integrity checks\n" \
	+ "                                  exit 0 = clean, exit 1 = parse failure or issues\n" \
	+ "  --vortaris-csv-stats <file>      print headers, inferred column types, row/col stats\n" \
	+ "                                  exit 0 on success, exit 1 on unrecoverable error\n" \
	+ "Example:\n" \
	+ "  godot --headless --path demo --script res://scripts/cli_entry.gd -- \\\n" \
	+ "      --vortaris-csv-validate res://data/monsters.csv"


func _init() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		_usage_and_quit(1)
		return
	match args[0]:
		"--vortaris-csv-validate":
			if args.size() < 2:
				_usage_and_quit(1)
				return
			quit(_cmd_validate(args[1]))
		"--vortaris-csv-stats":
			if args.size() < 2:
				_usage_and_quit(1)
				return
			quit(_cmd_stats(args[1]))
		_:
			print("[vortariscsv] unknown argument: ", args[0])
			_usage_and_quit(1)


func _usage_and_quit(code: int) -> void:
	print(USAGE)
	quit(code)


# Parse the CSV and run data-integrity checks. Exit 0 = clean, 1 = failure/issues.
func _cmd_validate(path: String) -> int:
	print("[vortariscsv] validate ", path)
	var r := VCSVParser.parse_file(path, null)
	if r == null or not r.success:
		var msg := "unknown parse error"
		if r != null and not r.message.is_empty():
			msg = r.message
		print("[vortariscsv]   ERROR: ", msg)
		if r != null and r.error_line > 0:
			print("[vortariscsv]   location: line ", r.error_line, ", col ", r.error_column)
		return 1

	var headers := r.table.headers
	var rows := r.table.rows
	print("[vortariscsv]   rows: ", rows.size())
	print("[vortariscsv]   columns: ", headers.size())
	print("[vortariscsv]   headers: ", headers)

	print("[vortariscsv]   parse warnings: ", r.warnings.size())
	for w in r.warnings:
		print("[vortariscsv]     warning: ", w)

	# Data-integrity checks without a row_type: duplicate keys (first column is
	# the default key column) are the meaningful structural check from the CLI.
	var t := VCSVDataTable.new()
	t.headers = headers
	t.rows = rows
	if not headers.is_empty():
		t.key_column = headers[0]
	var issues := t.validate()
	print("[vortariscsv]   validation issues: ", issues.size())
	for issue in issues:
		print("[vortariscsv]     - ", issue)

	if r.warnings.is_empty() and issues.is_empty():
		print("[vortariscsv] validate OK")
		return 0
	print("[vortariscsv] validate FAILED")
	return 1


# Print headers, inferred column types and row/column statistics. Exit 0.
func _cmd_stats(path: String) -> int:
	print("[vortariscsv] stats ", path)
	var r := VCSVParser.parse_file(path, null)
	if r == null or not r.success:
		var msg := "unknown parse error"
		if r != null and not r.message.is_empty():
			msg = r.message
		print("[vortariscsv]   ERROR: ", msg)
		if r != null and r.error_line > 0:
			print("[vortariscsv]   location: line ", r.error_line, ", col ", r.error_column)
		return 1

	var headers := r.table.headers
	var rows := r.table.rows
	print("[vortariscsv]   rows: ", rows.size())
	print("[vortariscsv]   columns: ", headers.size())
	print("[vortariscsv]   headers: ", headers)

	print("[vortariscsv]   column types:")
	var types := VCSVUtil.detect_types(r.table, ";", true)
	for c in headers.size():
		var h: String = headers[c]
		print("[vortariscsv]     ", h, ": ", types.get(h, "string"))

	print("[vortariscsv]   per-column stats (non_empty / numeric / distinct):")
	for c in headers.size():
		var h: String = headers[c]
		var st := r.table.column_stats(c)
		print("[vortariscsv]     ", h, ": non_empty=", st.get("non_empty"),
				" numeric=", st.get("numeric"), " distinct=", st.get("distinct"))

	print("[vortariscsv] stats done")
	return 0
