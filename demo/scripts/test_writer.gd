extends SceneTree

# Regression tests for the VCSVWriter (serialization / quoting / round-trip).
# Run: Godot --headless --path demo --script res://scripts/test_writer.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func test_quoting_rules() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	# Simple fields: no quoting.
	check(w.write_rows_to_string([[("a"), ("b")], [("c"), ("d")]]) == "a,b\nc,d\n",
			"simple rows unquoted")
	# Field with delimiter must be quoted.
	check(w.write_rows_to_string([[("a,b"), ("x")]]) == "\"a,b\",x\n",
			"comma field quoted")
	# Field with quote must be quoted and doubled.
	check(w.write_rows_to_string([[("say \"hi\"")]]) == "\"say \"\"hi\"\"\"\n",
			"quotes doubled")
	# Field with newline must be quoted.
	check(w.write_rows_to_string([[("a\nb")]]) == "\"a\nb\"\n",
			"newline field quoted")


func test_line_endings() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\r\n"
	check(w.write_rows_to_string([[("1"), ("2")]]) == "1,2\r\n", "CRLF default")
	w.line_ending = "\n"
	check(w.write_rows_to_string([[("1"), ("2")]]) == "1,2\n", "LF option")


func test_always_quote() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	w.always_quote = true
	check(w.write_rows_to_string([[("a"), ("b")]]) == "\"a\",\"b\"\n", "always quote")


func test_sanitize_formulas() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	w.sanitize_formulas = true
	check(w.write_rows_to_string([[("=SUM(A1)"), ("+1"), ("@x"), ("-5"), ("plain")]]) ==
			"'=SUM(A1),'+1,'@x,-5,plain\n",
			"formula fields prefixed (dash left alone)")


func test_roundtrip() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var src := "name,note\n\"a,b\",\"say \"\"hi\"\"\"\nline3,last\n"
	var t1 := VCSVParser.parse_string(src, null).table
	check(t1 != null, "parse ok")
	var out := w.write_table_to_string(t1)
	check(out == "name,note\n\"a,b\",\"say \"\"hi\"\"\"\nline3,last\n", "write reproduces input")
	# Re-parse: identical grid.
	var t2 := VCSVParser.parse_string(out, null).table
	check(t2.get_headers() == t1.get_headers(), "round-trip headers equal")
	check(t2.get_row_count() == t1.get_row_count(), "round-trip row count")
	for i in t1.get_row_count():
		check(t2.get_row(i) == t1.get_row(i), "round-trip row %d" % i)


func test_from_dicts() -> void:
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var dicts := [
		{"id": "1", "name": "goblin"},
		{"id": "2", "name": "orc"},
	]
	var out := w.from_dicts_to_string(dicts, PackedStringArray(["id", "name"]))
	check(out == "id,name\n1,goblin\n2,orc\n", "dicts to csv with explicit order")
	var auto := w.from_dicts_to_string(dicts)
	check(auto == "id,name\n1,goblin\n2,orc\n", "dicts to csv with auto order")


func test_quote_field() -> void:
	check(VCSVWriter.quote_field("plain") == "plain", "quote_field plain")
	check(VCSVWriter.quote_field("a,b") == "\"a,b\"", "quote_field comma")
	check(VCSVWriter.quote_field("say \"hi\"") == "\"say \"\"hi\"\"\"", "quote_field quotes")


func test_write_file() -> void:
	var path := "user://test_writer_tmp.csv"
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var err := w.write_rows([[("1"), ("2")]], path, PackedStringArray(["a", "b"]))
	check(err == OK, "write file ok")
	var back := VCSVParser.parse_file(path, null)
	check(back.success, "re-read file ok")
	check(back.table.get_headers() == PackedStringArray(["a", "b"]), "file headers")
	check(back.table.get_row(0) == PackedStringArray(["1", "2"]), "file row")


func _init() -> void:
	test_quoting_rules()
	test_line_endings()
	test_always_quote()
	test_sanitize_formulas()
	test_roundtrip()
	test_from_dicts()
	test_quote_field()
	test_write_file()
	if failures == 0:
		print("test_writer OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_writer FAILED: ", failures, "/", checks, " failed")
		quit(1)
