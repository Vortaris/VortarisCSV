extends SceneTree

# RFC 4180 / edge-case regression tests for the low-level VCSVParser.
# Run: Godot --headless --path demo --script res://scripts/test_parser.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func parse(text: String, opts: Variant = null) -> VCSVParseResult:
	var o: Variant = opts
	if o == null:
		o = VCSVParseOptions.new()
	return VCSVParser.parse_string(text, o)


func test_basic() -> void:
	var r := parse("a,b,c\n1,2,3\n4,5,6")
	check(r.success, "basic parse succeeds")
	check(r.error == OK, "basic error is OK")
	check(r.table != null, "table present")
	check(r.table.headers == PackedStringArray(["a", "b", "c"]), "headers correct")
	check(r.table.get_row_count() == 2, "2 data rows")
	check(r.table.get_row(0) == PackedStringArray(["1", "2", "3"]), "row 0 correct")
	check(r.table.get_value(0, "b") == "2", "get_value by header name")
	check(r.table.get_value(0, 2) == "3", "get_value by column index")
	check(r.table.get_col_count() == 3, "3 columns")
	check(r.table.get_row_dict(0) == {"a": "1", "b": "2", "c": "3"}, "row dict correct")
	check(r.table.to_dict_array().size() == 2, "to_dict_array size")


func test_quotes() -> void:
	var r := parse("name,note\n\"a,b\",\"say \"\"hi\"\"\"")
	check(r.success, "quoted parse succeeds")
	check(r.table.get_row(0) == PackedStringArray(["a,b", "say \"hi\""]),
			"quoted comma + escaped quotes")
	# Multi-line quoted field.
	var r2 := parse("x,y\n\"line1\nline2\",42")
	check(r2.success, "multi-line quoted field succeeds")
	check(r2.table.get_row(0)[0] == "line1\nline2", "newline inside quotes preserved")
	# Unterminated quote is a hard error with location.
	var r3 := parse("a,b\n1,\"oops\n")
	check(not r3.success, "unterminated quote fails")
	check(r3.error == ERR_PARSE_ERROR, "parse error code")
	check(r3.error_line == 2, "error line reported")
	check(r3.message.length() > 0, "error message present")


func test_line_endings() -> void:
	var r := parse("a,b\r\n1,2\r\n")
	check(r.success, "CRLF handled")
	check(r.table.get_row_count() == 1, "CRLF row count")
	check(r.table.get_row(0) == PackedStringArray(["1", "2"]), "CRLF row content")
	var r2 := parse("a,b\r1,2\r")
	check(r2.table.get_row_count() == 1, "bare CR handled")
	var r3 := parse("a,b\n1,2\n")
	check(r3.table.get_row_count() == 1, "LF handled")
	var r4 := parse("a,b\n1,2") # no trailing newline
	check(r4.table.get_row_count() == 1, "no trailing newline")


func test_comments() -> void:
	var o := VCSVParseOptions.new()
	o.comment_prefix = "#"
	var r := parse("# a comment line\na,b\n1,2\n", o)
	check(r.success, "comments skipped")
	check(r.table.get_row_count() == 1, "comment line not in data")
	check(r.table.headers == PackedStringArray(["a", "b"]), "comment before header ok")
	var r2 := parse("#only a comment\n", o)
	check(r2.success, "file with only comments")
	check(r2.table.get_row_count() == 0, "no data rows")


func test_strictness() -> void:
	var strict := VCSVParseOptions.new()
	strict.strict = true
	var r := parse("a,b\n1,2\n3\n", strict)
	check(not r.success, "strict rejects uneven rows")
	check(r.error_line == 3, "strict reports line")
	var lenient := VCSVParseOptions.new()
	lenient.strict = false
	var r2 := parse("a,b\n1\n", lenient)
	check(r2.success, "lenient accepts uneven")
	check(r2.warnings.size() > 0, "lenient records a warning")
	check(r2.table.get_row(0) == PackedStringArray(["1", ""]), "lenient pads short row")
	var r3 := parse("a,b\n1,2,3\n", lenient)
	check(r3.success, "lenient accepts long row")
	check(r3.table.get_row(0) == PackedStringArray(["1", "2"]), "lenient truncates long row")


func test_trim() -> void:
	var o := VCSVParseOptions.new()
	o.trim_whitespace = true
	var r := parse("a,b\n 1 ,  2  \n", o)
	check(r.table.get_row(0) == PackedStringArray(["1", "2"]), "trim unquoted whitespace")
	var r2 := parse("a,b\n \" x \" ,y\n", o)
	check(r2.table.get_row(0) == PackedStringArray([" x ", "y"]), "quoted content not trimmed")
	var no_trim := VCSVParseOptions.new()
	no_trim.trim_whitespace = false
	var r3 := parse("a,b\n 1 , 2 \n", no_trim)
	check(r3.table.get_row(0) == PackedStringArray([" 1 ", " 2 "]), "no trim preserves spaces")


func test_bom_and_empty() -> void:
	var r := parse("﻿a,b\n1,2")
	check(r.success, "BOM stripped")
	check(r.table.headers[0] == "a", "header not polluted by BOM")
	var r2 := parse("")
	check(r2.success, "empty text parses OK")
	check(r2.table.get_row_count() == 0, "empty text has no rows")
	var r3 := parse("a,b\n\n\n1,2\n")
	check(r3.success, "blank lines skipped")
	check(r3.table.get_row_count() == 1, "blank lines not data")


func test_no_header_and_single_col() -> void:
	var o := VCSVParseOptions.new()
	o.has_header = false
	var r := parse("1,2\n3,4", o)
	check(r.table.headers.is_empty(), "no headers when has_header=false")
	check(r.table.get_row_count() == 2, "all rows are data")
	var r2 := parse("name\nalice\nbob")
	check(r2.table.get_row_count() == 2, "single column with header")


func test_parse_file() -> void:
	var path := "user://test_parser_tmp.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string("id,val\nk1,10\n")
	f.close()
	var r := VCSVParser.parse_file(path, null)
	check(r.success, "parse_file succeeds")
	check(r.table.get_row_count() == 1, "parse_file row count")
	# UTF-8 BOM at byte level.
	var f2 := FileAccess.open(path, FileAccess.WRITE)
	f2.store_buffer(PackedByteArray([0xEF, 0xBB, 0xBF]) + "a,b\n1,2\n".to_utf8_buffer())
	f2.close()
	var r2 := VCSVParser.parse_file(path, null)
	check(r2.success, "parse_file with byte BOM succeeds")
	check(r2.table.headers[0] == "a", "byte BOM stripped from header")
	var missing := VCSVParser.parse_file("user://does_not_exist.csv", null)
	check(not missing.success, "missing file fails")
	check(missing.error == ERR_FILE_NOT_FOUND, "missing file error code")


func test_delimiter_auto_detect() -> void:
	# Semicolon-delimited file with auto-detect on (default candidates).
	var src := "id;name;hp\nk1;goblin;100\nk2;orc;80\n"
	var o := VCSVParseOptions.new()
	o.auto_detect_delimiter = true
	var r := VCSVParser.parse_string(src, o)
	check(r.success, "auto-detect parse succeeds")
	check(r.table.headers == PackedStringArray(["id", "name", "hp"]), "auto-detect headers")
	check(r.table.get_row_count() == 2, "auto-detect rows")
	check(r.table.get_row(0) == PackedStringArray(["k1", "goblin", "100"]), "auto-detect row content")

	# Pipe-delimited file.
	var src2 := "a|b|c\n1|2|3\n4|5|6\n"
	var o2 := VCSVParseOptions.new()
	o2.auto_detect_delimiter = true
	var r2 := VCSVParser.parse_string(src2, o2)
	check(r2.success, "pipe auto-detect")
	check(r2.table.headers == PackedStringArray(["a", "b", "c"]), "pipe headers")
	check(r2.table.get_row(0) == PackedStringArray(["1", "2", "3"]), "pipe row")

	# Comma file with quoted commas still detects comma (quote-aware width).
	var src3 := "a,b\n\"x,y\",1\n\"p,q\",2\n"
	var o3 := VCSVParseOptions.new()
	o3.auto_detect_delimiter = true
	var r3 := VCSVParser.parse_string(src3, o3)
	check(r3.success, "quote-aware auto-detect")
	check(r3.table.get_row(0) == PackedStringArray(["x,y", "1"]), "quoted comma preserved")

	# Auto-detect overrides a misconfigured explicit delimiter.
	var o4 := VCSVParseOptions.new()
	o4.auto_detect_delimiter = true
	o4.delimiter = ";"
	var r4 := VCSVParser.parse_string("a,b\n1,2\n", o4)
	check(r4.success, "auto-detect parse with explicit fallback")
	check(r4.table.headers == PackedStringArray(["a", "b"]), "auto-detect detects comma over explicit ';'")


func test_header_rows() -> void:
	var src := "Level,Level,Name\nHealth,Attack,-\n100,10,goblin\n80,20,orc\n"
	var o := VCSVParseOptions.new()
	o.header_rows = 2
	o.header_join = "."
	var r := VCSVParser.parse_string(src, o)
	check(r.success, "multi-header parse succeeds")
	check(r.table.headers == PackedStringArray(["Level.Health", "Level.Attack", "Name.-"]),
			"multi-header joined with '.'")
	check(r.table.get_row_count() == 2, "multi-header data rows")
	check(r.table.get_row(0) == PackedStringArray(["100", "10", "goblin"]), "multi-header row 0")

	# Custom join separator.
	var o2 := VCSVParseOptions.new()
	o2.header_rows = 2
	o2.header_join = "/"
	var r2 := VCSVParser.parse_string("A,B\nx,y\n1,2\n", o2)
	check(r2.success, "custom join parse")
	check(r2.table.headers == PackedStringArray(["A/x", "B/y"]), "custom join headers")


func test_max_errors() -> void:
	# 0.4.0: max_errors is honoured. Strict parses still FAIL on any hard error
	# (contract preserved), but below the budget the parser continues and collects
	# every problem as a "line:col error: ..." warning instead of stopping at #1.
	var src := "a,b\n1,2\n3\n4,5,6\n7,8\n"
	var o := VCSVParseOptions.new()
	o.strict = true
	var r := VCSVParser.parse_string(src, o)
	check(not r.success, "strict parse with width mismatches still fails")
	check(r.warnings.size() >= 2, "multiple hard errors collected as warnings (got %d)" % r.warnings.size())

	# Budget of 1: abort on the very first hard error.
	var o2 := VCSVParseOptions.new()
	o2.strict = true
	o2.max_errors = 1
	var r2 := VCSVParser.parse_string(src, o2)
	check(not r2.success, "budget=1 strict parse fails")
	check(r2.error_line >= 3, "budget=1 aborts at the first bad row (line %d)" % r2.error_line)

	# Lenient mode is untouched by the budget: width issues stay warnings, OK.
	var o3 := VCSVParseOptions.new()
	o3.max_errors = 1
	var r3 := VCSVParser.parse_string(src, o3)
	check(r3.success, "lenient parse unaffected by max_errors")
	check(r3.table.get_row_count() == 4, "lenient rows kept (padded/truncated, not dropped)")


func _init() -> void:
	test_basic()
	test_quotes()
	test_line_endings()
	test_comments()
	test_strictness()
	test_trim()
	test_bom_and_empty()
	test_no_header_and_single_col()
	test_parse_file()
	test_delimiter_auto_detect()
	test_header_rows()
	test_max_errors()
	if failures == 0:
		print("test_parser OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_parser FAILED: ", failures, "/", checks, " failed")
		quit(1)
