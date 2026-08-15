extends SceneTree

# Regression tests for the type conversion engine (VCSVUtil.detect_types /
# load_csv_dict_array / type_name).
# Run: Godot --headless --path demo --script res://scripts/test_types.gd

var failures := 0
var checks := 0


func check(cond: bool, msg: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("FAIL: ", msg)


func test_detect_types() -> void:
	var src := "id,hp,ratio,active,name,pos,color,items,data\n" + \
			"1,100,1.5,true,goblin,\"10,20\",#ff0000,\"1;2;3\",{\"a\":1}\n" + \
			"2,50,0.5,false,orc,\"30,40\",#00ff00,\"4;5\",{\"b\":2}\n"
	var table := VCSVParser.parse_string(src, null).table
	var types := VCSVUtil.detect_types(table, ";", true)
	check(types["id"] == "int", "id -> int")
	check(types["hp"] == "int", "hp -> int")
	check(types["ratio"] == "float", "ratio -> float")
	check(types["active"] == "bool", "active -> bool")
	check(types["name"] == "string", "name -> string")
	check(types["pos"] == "Vector2", "pos -> Vector2")
	check(types["color"] == "Color", "color -> Color")
	check(types["items"] == "int[]", "items -> int[]")
	check(types["data"] == "json", "data -> json")


func test_typed_dicts() -> void:
	var path := "user://test_types.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string("id,hp,ratio,active,name,pos,items\n")
	f.store_string("1,100,1.5,true,goblin,\"10,20\",\"1;2\"\n")
	f.close()

	var arr := VCSVUtil.load_csv_dict_array(path)
	check(arr.size() == 1, "one row loaded")
	var d: Dictionary = arr[0]
	check(typeof(d["id"]) == TYPE_INT and d["id"] == 1, "id typed int")
	check(typeof(d["hp"]) == TYPE_INT and d["hp"] == 100, "hp typed int")
	check(typeof(d["ratio"]) == TYPE_FLOAT and d["ratio"] == 1.5, "ratio typed float")
	check(d["active"] == true and typeof(d["active"]) == TYPE_BOOL, "active typed bool")
	check(d["name"] == "goblin", "name string")
	check(d["pos"] == Vector2(10, 20) and typeof(d["pos"]) == TYPE_VECTOR2, "pos typed Vector2")
	check(d["items"] == [1, 2], "items array of ints")


func test_json_and_color_dicts() -> void:
	var path := "user://test_types2.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string("name,color,data\n")
	f.store_string("sword,#ff8000,{\"dmg\":5}\n")
	f.close()
	var arr := VCSVUtil.load_csv_dict_array(path)
	var d: Dictionary = arr[0]
	check(d["color"] == Color("#ff8000"), "color parsed")
	var data: Dictionary = d["data"]
	check(data["dmg"] == 5, "json dict parsed")


func test_type_name() -> void:
	check(VCSVUtil.type_name(5) == "int", "type_name int")
	check(VCSVUtil.type_name(1.5) == "float", "type_name float")
	check(VCSVUtil.type_name(true) == "bool", "type_name bool")
	check(VCSVUtil.type_name(Vector3.ONE) == "Vector3", "type_name Vector3")


func test_missing_file() -> void:
	var arr := VCSVUtil.load_csv_dict_array("user://no_such_file.csv")
	check(arr.is_empty(), "missing file yields empty array")


func test_load_csv_dict() -> void:
	var path := "user://test_load_csv_dict.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string("id,hp,active,name\nk1,100,true,goblin\nk2,50,false,orc\n")
	f.close()

	var d := VCSVUtil.load_csv_dict(path)
	check(not d.is_empty(), "load_csv_dict returns first row")
	check(d["id"] == "k1", "load_csv_dict id")
	check(typeof(d["hp"]) == TYPE_INT and d["hp"] == 100, "load_csv_dict hp typed int")
	check(d["active"] == true and typeof(d["active"]) == TYPE_BOOL, "load_csv_dict active bool")
	check(d["name"] == "goblin", "load_csv_dict name")

	# Same behavior as load_csv_dict_array's first row.
	var arr := VCSVUtil.load_csv_dict_array(path)
	check(not arr.is_empty(), "dict_array non-empty")
	check((arr[0] as Dictionary) == d, "load_csv_dict == first dict row")

	var empty := VCSVUtil.load_csv_dict("user://no_such_file.csv")
	check(empty.is_empty(), "load_csv_dict missing file -> empty dict")


func test_header_schema() -> void:
	var src := "id,hp:int,ratio:float,active:bool,plain\nk1,100,1.5,true,xyz\nk2,50,0.5,false,abc\n"
	var opts := VCSVParseOptions.new()
	opts.header_type_separator = ":"
	var r := VCSVParser.parse_string(src, opts)
	check(r.success, "schema parse ok")
	check(r.table.headers == PackedStringArray(["id", "hp", "ratio", "active", "plain"]),
			"schema annotations stripped from headers")
	var ct: Dictionary = r.column_types
	check(ct["hp"] == "int", "schema hp -> int")
	check(ct["ratio"] == "float", "schema ratio -> float")
	check(ct["active"] == "bool", "schema active -> bool")

	# load_csv_dict_array applies the schema types.
	var path := "user://test_header_schema.csv"
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(src)
	f.close()
	var arr := VCSVUtil.load_csv_dict_array(path, opts)
	check(arr.size() == 2, "schema rows loaded")
	var d: Dictionary = arr[0]
	check(typeof(d["hp"]) == TYPE_INT and d["hp"] == 100, "schema hp typed int")
	check(typeof(d["ratio"]) == TYPE_FLOAT and d["ratio"] == 1.5, "schema ratio typed float")
	check(d["active"] == true and typeof(d["active"]) == TYPE_BOOL, "schema active typed bool")
	check(d["plain"] == "xyz", "schema unannotated column stays string")

	# Invalid type suffix falls back to string (annotation still stripped).
	var src2 := "id,weird:notatype\nk1,val\n"
	var r2 := VCSVParser.parse_string(src2, opts)
	check(r2.success, "invalid schema parse ok")
	check(r2.table.headers == PackedStringArray(["id", "weird"]), "invalid annotation still stripped")
	check(r2.column_types["weird"] == "string", "invalid type falls back to string")

	# Separator off by default: "hp:int" stays a plain header.
	var plain := VCSVParser.parse_string("id,hp:int\nk1,100\n", null)
	check(plain.table.headers == PackedStringArray(["id", "hp:int"]), "separator off keeps ':' in headers")
	check(plain.column_types.is_empty(), "no column_types when separator off")


func _init() -> void:
	test_detect_types()
	test_typed_dicts()
	test_json_and_color_dicts()
	test_type_name()
	test_missing_file()
	test_load_csv_dict()
	test_header_schema()
	if failures == 0:
		print("test_types OK: ", checks, " checks passed")
		quit(0)
	else:
		printerr("test_types FAILED: ", failures, "/", checks, " failed")
		quit(1)
