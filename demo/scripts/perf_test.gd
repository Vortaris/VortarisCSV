extends SceneTree

# Performance smoke test: parse and bind a large synthetic CSV.
# Soft targets: ~1M cells parse under ~2s, DataTable binding under ~4s.
# Run: Godot --headless --path demo --script res://scripts/perf_test.gd

const ROWS := 100_000
const COLS := 12

var failures := 0


func fail(msg: String) -> void:
	failures += 1
	printerr("FAIL: ", msg)


func build_csv() -> String:
	var b := PackedStringArray()
	var header := "id,a,b,c,flag,ratio,posx,posy,color,note,tags,extra\n"
	b.append(header)
	var row := "k%d,%d,%d,%.2f,%s,%.3f,%d,%d,#%02x%02x%02x,item_%d,1;2;3,x\n"
	for i in ROWS:
		b.append(row % [
			i, i, i * 2, i * 0.5,
			("true" if i % 2 == 0 else "false"),
			i * 0.25, i, i, i % 255, (i + 1) % 255, (i + 2) % 255, i,
		])
	return "\n".join(b)


func _init() -> void:
	var text := build_csv()
	print("csv bytes=", text.length())

	var t0 := Time.get_ticks_usec()
	var result := VCSVParser.parse_string(text, null)
	var t_parse := (Time.get_ticks_usec() - t0) / 1e6
	if not result.success:
		fail("parse failed: " + result.message)
		quit(1)
		return
	print("parse rows=", result.table.get_row_count(), " cols=", result.table.get_col_count(),
			" seconds=%.2f" % t_parse)
	if t_parse > 2.0:
		fail("parse too slow: %.2fs" % t_parse)

	var t1 := Time.get_ticks_usec()
	var dt := VCSVDataTable.new()
	dt.headers = result.table.headers
	dt.rows = result.table.rows
	dt.key_column = "id"
	dt.row_type = "res://scripts/row_types/monster_row.gd"
	dt.ensure_loaded()
	var t_bind := (Time.get_ticks_usec() - t1) / 1e6
	print("bind seconds=%.2f errors=%d" % [t_bind, dt.get_last_errors().size()])
	if t_bind > 4.0:
		fail("bind too slow: %.2fs" % t_bind)

	var t2 := Time.get_ticks_usec()
	var row := dt.get_row("k50000")
	var t_get := (Time.get_ticks_usec() - t2) / 1e6
	print("key lookup k50000 -> ", row != null, " (%.4fs)" % t_get)
	if row == null or row.get("id") != "k50000":
		fail("key lookup wrong")

	# Lazy build comparison: structure only, then on-demand row construction.
	var t3 := Time.get_ticks_usec()
	var dt_lazy := VCSVDataTable.new()
	dt_lazy.headers = result.table.headers
	dt_lazy.rows = result.table.rows
	dt_lazy.key_column = "id"
	dt_lazy.row_type = "res://scripts/row_types/monster_row.gd"
	dt_lazy.lazy_build = true
	dt_lazy.ensure_loaded()
	var t_lazy_struct := (Time.get_ticks_usec() - t3) / 1e6
	print("lazy struct seconds=%.2f (no typed rows built)" % t_lazy_struct)
	if t_lazy_struct > 1.0:
		fail("lazy struct too slow: %.2fs" % t_lazy_struct)

	var t4 := Time.get_ticks_usec()
	var lazy_row := dt_lazy.get_row("k50000")
	var t_lazy_get := (Time.get_ticks_usec() - t4) / 1e6
	print("lazy key lookup k50000 -> ", lazy_row != null, " (%.4fs)" % t_lazy_get)
	if lazy_row == null or lazy_row.get("id") != "k50000":
		fail("lazy key lookup wrong")
	# build_row caches: a second lookup must hit the cache.
	var t5 := Time.get_ticks_usec()
	var lazy_row2 := dt_lazy.get_row("k50000")
	var t_lazy_get2 := (Time.get_ticks_usec() - t5) / 1e6
	print("lazy cached lookup (%.4fs)" % t_lazy_get2)

	# Lazy and eager agree on values.
	if lazy_row.get("health") != row.get("health"):
		fail("lazy/eager value mismatch")

	print(failures == 0 and "perf_test OK" or "perf_test FAILED")
	quit(1 if failures > 0 else 0)
