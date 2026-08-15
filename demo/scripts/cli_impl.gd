extends RefCounted

# CLI implementation for cli_entry.gd. Kept in a separate file — and only loaded
# after cli_entry.gd's guard passes — so that cli_entry.gd itself never mentions a
# VCSV* identifier at parse time. On a fresh clone the GDExtension cache
# (.godot/extension_list.cfg) does not exist, those classes are absent, and an
# unresolved identifier is a hard GDScript parse error that aborts before the
# entry point's clear error message can be printed.

const USAGE := "[vortariscsv] usage:\n" \
	+ "  --vortaris-csv-validate <file>   parse the CSV + run data-integrity checks\n" \
	+ "                                  exit 0 = clean, exit 1 = parse failure or issues\n" \
	+ "  --vortaris-csv-stats <file>      print headers, inferred column types, row/col stats\n" \
	+ "                                  exit 0 on success, exit 1 on unrecoverable error\n" \
	+ "Example:\n" \
	+ "  godot --headless --path demo --script res://scripts/cli_entry.gd -- \\\n" \
	+ "      --vortaris-csv-validate res://data/monsters.csv"


# Parse the CSV and run data-integrity checks. Exit 0 = clean, 1 = failure/issues.
static func cmd_validate(path: String) -> int:
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
static func cmd_stats(path: String) -> int:
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
