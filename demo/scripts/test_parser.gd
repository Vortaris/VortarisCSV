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
	if failures == 0:
		print("test_parser OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_parser FAILED: ", failures, "/", checks, " failed")
		quit(1)
