@tool
class_name VCSVGrid
extends Control
## Excel-style self-drawn grid for the VortarisCSV editor.
##
## Godot has no built-in spreadsheet control and `Tree` reads as a tree (arrows,
## no cell borders). This control draws a real grid — header row, row-number
## gutter, cell borders, banded rows — with a single `_draw()` pass that only
## renders the visible region (virtual scrolling), so thousands of rows stay
## responsive.
##
## Features:
##   - Column headers (click to sort, drag the right edge to resize)
##   - Excel-style row numbers in a left gutter
##   - Cell selection + rectangle drag selection, Ctrl+C copies TSV
##   - Right-click context menu (copy cell / row / column / selection)
##   - Double-click a cell to edit (floating LineEdit overlay) — emits
##     `cell_edited(data_row, col, text)` where `data_row` is the ORIGINAL row
##     index regardless of the active sort order
##
## Data is handed in whole via `set_data(headers, rows)`; the grid never writes
## files itself.

signal cell_edited(data_row: int, col: int, text: String)
signal header_clicked(col: int)

const MIN_COL_WIDTH := 40
const DEFAULT_COL_WIDTH := 110
const CELL_PAD := 5.0
const SEPARATOR_TOLERANCE := 6.0

var _headers: PackedStringArray = PackedStringArray()
var _rows: Array = []           # Array[PackedStringArray]
var _col_w: PackedInt32Array = PackedInt32Array()
var _order: PackedInt32Array = PackedInt32Array()  # view row -> data row
var _sort_col := -1
var _sort_asc := true
var _editable := false

# Selection, in VIEW coordinates (row = position in _order, col = column index).
var _has_selection := false
var _sel_from := Vector2i(-1, -1)
var _sel_to := Vector2i(-1, -1)
var _dragging_selection := false

# Column resize state (mirrors VCSVResizableTree behaviour).
var _resizing_col := -1
var _resize_start_x := 0.0
var _resize_start_w := 0.0

# Scroll offsets in pixels (driven by the scrollbars).
var _scroll_x := 0.0
var _scroll_y := 0.0

var _vbar: VScrollBar
var _hbar: HScrollBar
var _menu: PopupMenu
var _editor: LineEdit
var _edit_view_row := -1
var _edit_col := -1

# Cached metrics (recomputed on theme/data changes).
var _row_h := 24.0
var _header_h := 26.0
var _rownum_w := 46.0


func _ready() -> void:
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	mouse_filter = Control.MOUSE_FILTER_STOP

	_vbar = VScrollBar.new()
	_vbar.anchor_left = 1.0
	_vbar.anchor_right = 1.0
	_vbar.anchor_top = 0.0
	_vbar.anchor_bottom = 1.0
	_vbar.offset_left = -16.0
	_vbar.offset_bottom = -16.0
	_vbar.value_changed.connect(_on_scroll_changed)
	add_child(_vbar)

	_hbar = HScrollBar.new()
	_hbar.anchor_left = 0.0
	_hbar.anchor_right = 1.0
	_hbar.anchor_top = 1.0
	_hbar.anchor_bottom = 1.0
	_hbar.offset_top = -16.0
	_hbar.offset_right = -16.0
	_hbar.value_changed.connect(_on_scroll_changed)
	add_child(_hbar)

	_menu = PopupMenu.new()
	_menu.add_item("Copy Cell", 0)
	_menu.add_item("Copy Selection", 1)
	_menu.add_item("Copy Row", 2)
	_menu.add_item("Copy Column", 3)
	_menu.add_item("Copy Header", 4)
	_menu.id_pressed.connect(_on_menu_item)
	add_child(_menu)

	_editor = LineEdit.new()
	_editor.visible = false
	add_child(_editor)
	_editor.text_submitted.connect(func(_t: String): _commit_edit(true, false))
	_editor.gui_input.connect(_on_editor_gui_input)

	resized.connect(_update_scroll_limits)
	gui_input.connect(_on_gui_input)
	_recompute_metrics()


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

func set_data(headers: PackedStringArray, rows: Array) -> void:
	_headers = headers
	_rows = rows
	_col_w = PackedInt32Array()
	for i in headers.size():
		_col_w.append(DEFAULT_COL_WIDTH)
	_order = PackedInt32Array()
	for i in rows.size():
		_order.append(i)
	_sort_col = -1
	_clear_selection()
	_commit_edit(false, false)
	_recompute_metrics()
	_update_scroll_limits()
	queue_redraw()


func clear() -> void:
	set_data(PackedStringArray(), [])


func set_editable(v: bool) -> void:
	_editable = v


func set_sort(col: int, asc: bool) -> void:
	if col < 0 or col >= _headers.size():
		return
	_sort_col = col
	_sort_asc = asc
	_apply_sort()
	_clear_selection()
	queue_redraw()


## TSV text of the current selection (view order), empty string when none.
func get_selected_text() -> String:
	if not _has_selection:
		return ""
	var r := _sel_rect()
	var lines: PackedStringArray = PackedStringArray()
	for vrow in range(r.position.y, r.position.y + r.size.y):
		if vrow < 0 or vrow >= _order.size():
			continue
		var data_row: int = _order[vrow]
		var row: PackedStringArray = _rows[data_row] if data_row < _rows.size() else PackedStringArray()
		var cells: PackedStringArray = PackedStringArray()
		for c in range(r.position.x, r.position.x + r.size.x):
			cells.append(_cell_text(row, c))
		lines.append("\t".join(cells))
	return "\n".join(lines)


func copy_selection_to_clipboard() -> void:
	var text := get_selected_text()
	if not text.is_empty():
		DisplayServer.clipboard_set(text)


## The full data rows (view order) touched by the current selection — used by
## "Export Rows". Returns Array[PackedStringArray].
func get_selected_data_rows() -> Array:
	var out: Array = []
	if not _has_selection:
		return out
	var r := _sel_rect()
	for vrow in range(r.position.y, r.position.y + r.size.y):
		if vrow < 0 or vrow >= _order.size():
			continue
		var data_row: int = _order[vrow]
		if data_row >= 0 and data_row < _rows.size():
			out.append(_rows[data_row])
	return out


# ---------------------------------------------------------------------------
# Metrics & scrolling
# ---------------------------------------------------------------------------

func _recompute_metrics() -> void:
	var font := get_theme_font("font")
	var fs := get_theme_font_size("font_size")
	var fh := float(font.get_height(fs))
	_row_h = fh + 7.0
	_header_h = fh + 10.0
	var digits := maxi(3, len(str(maxi(_rows.size(), 1))))
	_rownum_w = float(digits) * float(font.get_char_size("0".unicode_at(0), fs).x) + CELL_PAD * 2.0 + 4.0


func _total_w() -> float:
	var w := _rownum_w
	for cw in _col_w:
		w += float(cw)
	return w


func _total_h() -> float:
	return _header_h + _row_h * float(_rows.size())


func _view_w() -> float:
	return size.x - 16.0 - _rownum_w  # minus vbar width


func _view_h() -> float:
	return size.y - 16.0 - _header_h  # minus hbar height


func _update_scroll_limits() -> void:
	if _vbar == null or _hbar == null:
		return
	_vbar.max_value = maxf(0.0, _total_h() - _view_h())
	_vbar.page = maxf(1.0, _view_h())
	_vbar.visible = _vbar.max_value > 0.0
	_hbar.max_value = maxf(0.0, _total_w() - (_rownum_w + _view_w()))
	_hbar.page = maxf(1.0, _view_w())
	_hbar.visible = _hbar.max_value > 0.0
	_scroll_x = _hbar.value
	_scroll_y = _vbar.value
	queue_redraw()


func _on_scroll_changed(_v: float) -> void:
	_scroll_x = _hbar.value
	_scroll_y = _vbar.value
	queue_redraw()


# ---------------------------------------------------------------------------
# Drawing
# ---------------------------------------------------------------------------

func _notification(what: int) -> void:
	if what == NOTIFICATION_THEME_CHANGED:
		_recompute_metrics()
		queue_redraw()


func _draw() -> void:
	var font := get_theme_font("font")
	var fs := get_theme_font_size("font_size")
	var base_bg := get_theme_color("dark_color_2", "Editor") if has_theme_color("dark_color_2", "Editor") else Color(0.15, 0.15, 0.17)
	var header_bg := get_theme_color("dark_color_3", "Editor") if has_theme_color("dark_color_3", "Editor") else Color(0.12, 0.12, 0.14)
	var band_bg := base_bg.lightened(0.03)
	var grid_line := Color(1, 1, 1, 0.09)
	var text_c := get_theme_color("font_color")
	# 文本颜色按表格背景亮度自动反色:背景深配浅字、背景浅配深字,
	# 保证浅色编辑器主题(黑字)下黑字黑底依旧可读。
	var base_lum := base_bg.get_luminance()
	var text_lum := text_c.get_luminance()
	if base_lum >= 0.5 and text_lum >= 0.5:
		text_c = Color(0.12, 0.12, 0.14)  # 浅底配深字
	elif base_lum < 0.5 and text_lum < 0.5:
		text_c = Color(0.93, 0.93, 0.95)  # 深底配浅字
	var dim_c := Color(text_c, 0.55)
	var accent := get_theme_color("accent_color")
	var sel_fill := Color(accent.r, accent.g, accent.b, 0.28)
	var sel_border := Color(accent, 0.9)

	var ncols := _headers.size()
	if ncols == 0:
		return

	# --- data area geometry -------------------------------------------------
	var data_x := _rownum_w
	var data_y := _header_h
	var first_row := int(floor(_scroll_y / _row_h))
	var last_row := mini(_rows.size() - 1, first_row + int(ceil(_view_h() / _row_h)) + 1)

	# Column x positions (content coordinates), then visibility range.
	var col_x: Array = []  # left edge per column, content-space
	var x := 0.0
	for c in ncols:
		col_x.append(x)
		x += float(_col_w[c])
	var first_col := 0
	while first_col < ncols - 1 and col_x[first_col] + float(_col_w[first_col]) <= _scroll_x:
		first_col += 1
	var last_col := first_col
	while last_col < ncols - 1 and col_x[last_col] < _scroll_x + _view_w():
		last_col += 1

	var sel_r := _sel_rect() if _has_selection else Rect2i(-1, -1, 0, 0)

	# --- body cells ----------------------------------------------------------
	draw_rect(Rect2(data_x, data_y, size.x - data_x, size.y - data_y - 16.0), base_bg)
	for vrow in range(maxi(0, first_row), last_row + 1):
		if vrow >= _order.size():
			break
		var y := data_y + float(vrow) * _row_h - _scroll_y
		if y + _row_h < data_y:
			continue
		var data_row: int = _order[vrow]
		var row: PackedStringArray = _rows[data_row] if data_row < _rows.size() else PackedStringArray()
		if vrow % 2 == 1:
			draw_rect(Rect2(data_x, y, size.x - data_x, _row_h), band_bg)
		for c in range(first_col, last_col + 1):
			var cx: float = data_x + float(col_x[c]) - _scroll_x
			var rect := Rect2(cx, y, float(_col_w[c]), _row_h)
			if _has_selection and sel_r.has_point(Vector2i(c, vrow)):
				draw_rect(rect, sel_fill)
			draw_string(font, Vector2(cx + CELL_PAD, y + (_row_h + fh_ascent(font, fs)) * 0.5),
					_cell_text(row, c), HORIZONTAL_ALIGNMENT_LEFT,
					float(_col_w[c]) - CELL_PAD * 2.0, fs, text_c)
	# Body grid lines.
	for vrow in range(maxi(0, first_row), last_row + 2):
		var y := data_y + float(vrow) * _row_h - _scroll_y
		if y > size.y - 16.0:
			break
		draw_line(Vector2(data_x, y), Vector2(size.x - 16.0, y), grid_line, 1.0)
	for c in range(first_col, last_col + 2):
		if c > ncols:
			break
		var cx: float = data_x + (float(col_x[c]) if c < ncols else x) - _scroll_x
		draw_line(Vector2(cx, data_y), Vector2(cx, size.y - 16.0), grid_line, 1.0)

	# --- header row ----------------------------------------------------------
	draw_rect(Rect2(data_x, 0, size.x - data_x, _header_h), header_bg)
	for c in range(first_col, last_col + 1):
		var cx: float = data_x + float(col_x[c]) - _scroll_x
		var title := str(_headers[c]) if c < _headers.size() else ""
		if c == _sort_col:
			title += "  " + ("▲" if _sort_asc else "▼")
		draw_string(font, Vector2(cx + CELL_PAD, (_header_h + fh_ascent(font, fs)) * 0.5),
				title, HORIZONTAL_ALIGNMENT_LEFT,
				float(_col_w[c]) - CELL_PAD * 2.0, fs, text_c)
	# Header separators + bottom border.
	for c in range(first_col, last_col + 2):
		if c > ncols:
			break
		var cx: float = data_x + (float(col_x[c]) if c < ncols else x) - _scroll_x
		draw_line(Vector2(cx, 0), Vector2(cx, _header_h), grid_line, 1.0)
	draw_line(Vector2(0, _header_h), Vector2(size.x, _header_h), Color(1, 1, 1, 0.16), 1.0)

	# --- row-number gutter ---------------------------------------------------
	draw_rect(Rect2(0, data_y, _rownum_w, size.y - data_y - 16.0), header_bg)
	for vrow in range(maxi(0, first_row), last_row + 1):
		if vrow >= _order.size():
			break
		var y := data_y + float(vrow) * _row_h - _scroll_y
		if y + _row_h < data_y:
			continue
		draw_string(font, Vector2(CELL_PAD, y + (_row_h + fh_ascent(font, fs)) * 0.5),
				str(vrow + 1), HORIZONTAL_ALIGNMENT_RIGHT,
				_rownum_w - CELL_PAD * 2.0, fs, dim_c)
	draw_line(Vector2(_rownum_w, 0), Vector2(_rownum_w, size.y - 16.0), Color(1, 1, 1, 0.16), 1.0)
	# Corner cell.
	draw_rect(Rect2(0, 0, _rownum_w, _header_h), header_bg.darkened(0.1))


func fh_ascent(font: Font, fs: int) -> float:
	return float(font.get_ascent(fs))


func _cell_text(row: PackedStringArray, c: int) -> String:
	return row[c] if c >= 0 and c < row.size() else ""


# ---------------------------------------------------------------------------
# Input: selection, resize, sort, edit, wheel, keys
# ---------------------------------------------------------------------------

func _on_gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		match mb.button_index:
			MOUSE_BUTTON_LEFT:
				_on_left_button(mb)
			MOUSE_BUTTON_RIGHT:
				if mb.pressed:
					_open_menu(mb.position)
					accept_event()
			MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN:
				var bar := _hbar if mb.shift_pressed else _vbar
				var step := (_row_h * 3.0) if bar == _vbar else 60.0
				bar.value = clampf(bar.value + (-step if mb.button_index == MOUSE_BUTTON_WHEEL_UP else step), 0.0, bar.max_value)
				accept_event()
	elif event is InputEventMouseMotion:
		_on_motion(event as InputEventMouseMotion)
	elif event is InputEventKey:
		_on_key(event as InputEventKey)


func _on_left_button(mb: InputEventMouseButton) -> void:
	var pos := mb.position
	if mb.pressed:
		# Header separator drag?
		if pos.y <= _header_h:
			var sep := _separator_at(pos)
			if sep >= 0:
				_resizing_col = sep
				_resize_start_x = pos.x
				_resize_start_w = float(_col_w[sep])
				accept_event()
				return
			# Header click -> sort.
			var hcol := _col_at(pos)
			if hcol >= 0:
				if _sort_col == hcol:
					_sort_asc = not _sort_asc
				else:
					_sort_col = hcol
					_sort_asc = true
				_apply_sort()
				_clear_selection()
				header_clicked.emit(hcol)
				queue_redraw()
				accept_event()
				return
		# Gutter click: nothing (kept for visual completeness).
		if pos.x < _rownum_w or pos.y < _header_h:
			return
		var cell := _cell_at(pos)
		if cell.x < 0:
			return
		grab_focus()
		_commit_edit(true, false)
		if mb.double_click and _editable:
			_begin_edit(cell.y, cell.x)
			accept_event()
			return
		_sel_from = cell
		_sel_to = cell
		_has_selection = true
		_dragging_selection = true
		queue_redraw()
		accept_event()
	else:
		if _resizing_col >= 0:
			_resizing_col = -1
			accept_event()
		elif _dragging_selection:
			_dragging_selection = false
			accept_event()


func _on_motion(mm: InputEventMouseMotion) -> void:
	if _resizing_col >= 0:
		if not (mm.button_mask & MOUSE_BUTTON_MASK_LEFT):
			_resizing_col = -1
			return
		var new_w := int(maxf(_resize_start_w + (mm.position.x - _resize_start_x), float(MIN_COL_WIDTH)))
		if _resizing_col < _col_w.size():
			_col_w[_resizing_col] = new_w
			_update_scroll_limits()
		accept_event()
		return
	if _dragging_selection and (mm.button_mask & MOUSE_BUTTON_MASK_LEFT):
		var cell := _cell_at_clamped(mm.position)
		if cell.x >= 0:
			_sel_to = cell
			queue_redraw()
		accept_event()
		return
	# Cursor shape over header separators.
	if mm.position.y <= _header_h:
		var want := Control.CURSOR_HSIZE if _separator_at(mm.position) >= 0 else Control.CURSOR_ARROW
		if mouse_default_cursor_shape != want:
			mouse_default_cursor_shape = want


func _on_key(key: InputEventKey) -> void:
	if not key.pressed or key.echo:
		return
	if key.keycode == KEY_C and key.ctrl_pressed:
		copy_selection_to_clipboard()
		accept_event()
		return
	if not _has_selection:
		return
	var r := _sel_rect()
	var move := Vector2i.ZERO
	match key.keycode:
		KEY_UP: move = Vector2i(0, -1)
		KEY_DOWN: move = Vector2i(0, 1)
		KEY_LEFT: move = Vector2i(-1, 0)
		KEY_RIGHT: move = Vector2i(1, 0)
		_: return
	var target := Vector2i(
			clampi(_sel_to.x + move.x, 0, maxi(0, _headers.size() - 1)),
			clampi(_sel_to.y + move.y, 0, maxi(0, _rows.size() - 1)))
	_sel_from = target
	_sel_to = target
	ensure_cell_visible(target.y, target.x)
	queue_redraw()
	accept_event()


func ensure_cell_visible(vrow: int, col: int) -> void:
	if col >= 0 and col < _col_w.size():
		var x0 := _col_x_of(col)
		var x1 := x0 + float(_col_w[col])
		if x0 < _scroll_x:
			_hbar.value = x0
		elif x1 > _scroll_x + _view_w():
			_hbar.value = x1 - _view_w()
	var y0 := float(vrow) * _row_h
	var y1 := y0 + _row_h
	if y0 < _scroll_y:
		_vbar.value = y0
	elif y1 > _scroll_y + _view_h():
		_vbar.value = y1 - _view_h()


# ---------------------------------------------------------------------------
# Hit testing / geometry helpers
# ---------------------------------------------------------------------------

func _col_x_of(col: int) -> float:
	var x := 0.0
	for c in col:
		x += float(_col_w[c])
	return x


func _col_at(pos: Vector2) -> int:
	var x := pos.x - _rownum_w + _scroll_x
	if x < 0:
		return -1
	var acc := 0.0
	for c in _col_w.size():
		acc += float(_col_w[c])
		if x < acc:
			return c
	return -1


func _cell_at(pos: Vector2) -> Vector2i:
	var col := _col_at(pos)
	if col < 0 or pos.y < _header_h:
		return Vector2i(-1, -1)
	var vrow := int(floor((pos.y - _header_h + _scroll_y) / _row_h))
	if vrow < 0 or vrow >= _rows.size():
		return Vector2i(-1, -1)
	return Vector2i(col, vrow)


func _cell_at_clamped(pos: Vector2) -> Vector2i:
	var col := _col_at(pos)
	if col < 0:
		return Vector2i(-1, -1)
	var vrow := int(floor((pos.y - _header_h + _scroll_y) / _row_h))
	vrow = clampi(vrow, 0, maxi(0, _rows.size() - 1))
	return Vector2i(col, vrow)


func _separator_at(pos: Vector2) -> int:
	var x := pos.x - _rownum_w + _scroll_x
	var acc := 0.0
	for c in _col_w.size():
		acc += float(_col_w[c])
		if absf(x - acc) <= SEPARATOR_TOLERANCE:
			return c
	return -1


func _sel_rect() -> Rect2i:
	if not _has_selection:
		return Rect2i(-1, -1, 0, 0)
	var x0 := mini(_sel_from.x, _sel_to.x)
	var y0 := mini(_sel_from.y, _sel_to.y)
	var x1 := maxi(_sel_from.x, _sel_to.x)
	var y1 := maxi(_sel_from.y, _sel_to.y)
	return Rect2i(x0, y0, x1 - x0 + 1, y1 - y0 + 1)


func _clear_selection() -> void:
	_has_selection = false
	_sel_from = Vector2i(-1, -1)
	_sel_to = Vector2i(-1, -1)
	_dragging_selection = false


# ---------------------------------------------------------------------------
# Sorting
# ---------------------------------------------------------------------------

func _apply_sort() -> void:
	if _sort_col < 0 or _sort_col >= _headers.size():
		return
	var col := _sort_col
	var asc := _sort_asc
	var idx: Array = []
	for i in _order.size():
		idx.append(int(_order[i]))
	idx.sort_custom(func(a: int, b: int) -> bool:
		var ta := _row_cell(a, col)
		var tb := _row_cell(b, col)
		var fa := _try_float(ta)
		var fb := _try_float(tb)
		var less: bool
		if not is_nan(fa) and not is_nan(fb):
			less = fa < fb
		elif ta == tb:
			less = false
		else:
			less = ta.naturalnocasecmp_to(tb) < 0
		if ta == tb or (not is_nan(fa) and not is_nan(fb) and fa == fb):
			return a < b if asc else a > b  # stable tie-break
		return less if asc else not less)
	_order = PackedInt32Array()
	for i in idx:
		_order.append(i)


func _row_cell(data_row: int, col: int) -> String:
	if data_row < 0 or data_row >= _rows.size():
		return ""
	var row: PackedStringArray = _rows[data_row]
	return row[col] if col >= 0 and col < row.size() else ""


func _try_float(s: String) -> float:
	var t := s.strip_edges()
	if t.is_empty():
		return NAN
	if t.is_valid_float():
		return t.to_float()
	return NAN


# ---------------------------------------------------------------------------
# Editing (floating LineEdit overlay)
# ---------------------------------------------------------------------------

func _begin_edit(vrow: int, col: int) -> void:
	if not _editable or vrow < 0 or vrow >= _order.size() or col < 0 or col >= _col_w.size():
		return
	_edit_view_row = vrow
	_edit_col = col
	var data_row: int = _order[vrow]
	_editor.text = _row_cell(data_row, col)
	var rect := _cell_screen_rect(vrow, col)
	_editor.position = rect.position
	_editor.size = Vector2(rect.size.x, maxf(rect.size.y, 24.0))
	_editor.visible = true
	_editor.grab_focus()
	_editor.caret_column = _editor.text.length()


func _cell_screen_rect(vrow: int, col: int) -> Rect2:
	var x := _rownum_w + _col_x_of(col) - _scroll_x
	var y := _header_h + float(vrow) * _row_h - _scroll_y
	return Rect2(x, y, float(_col_w[col]), _row_h)


func _commit_edit(apply: bool, keep_focus_grid: bool) -> void:
	if _edit_view_row < 0:
		return
	var vrow := _edit_view_row
	var col := _edit_col
	_edit_view_row = -1
	_edit_col = -1
	_editor.visible = false
	if apply and vrow >= 0 and vrow < _order.size():
		var data_row: int = _order[vrow]
		var new_text := _editor.text
		if new_text != _row_cell(data_row, col):
			cell_edited.emit(data_row, col, new_text)
			# The main screen writes back + refreshes; update our own copy too so
			# the grid shows the change immediately even before a full refresh.
			if data_row < _rows.size():
				var row: PackedStringArray = _rows[data_row]
				while row.size() <= col:
					row.append("")
				row[col] = new_text
				_rows[data_row] = row
			queue_redraw()
	if keep_focus_grid:
		grab_focus()


func _on_editor_gui_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_ESCAPE:
			_commit_edit(false, true)
			accept_event()
		elif event.keycode == KEY_TAB:
			_commit_edit(true, false)
			accept_event()


# ---------------------------------------------------------------------------
# Context menu
# ---------------------------------------------------------------------------

var _menu_cell := Vector2i(-1, -1)


func _open_menu(pos: Vector2) -> void:
	if pos.y <= _header_h and pos.x > _rownum_w:
		_menu_cell = Vector2(_col_at(pos), -1)
	else:
		var cell := _cell_at(pos)
		if cell.x < 0:
			return
		if not (_has_selection and _sel_rect().has_point(Vector2i(cell.x, cell.y))):
			_sel_from = cell
			_sel_to = cell
			_has_selection = true
			queue_redraw()
		_menu_cell = cell
	_menu.set_item_disabled(1, not _has_selection)
	_menu.set_item_disabled(4, _menu_cell.y != -1)
	_menu.popup(Rect2(Vector2(get_global_transform() * pos), Vector2.ZERO))


func _on_menu_item(id: int) -> void:
	var text := ""
	match id:
		0:  # Copy cell
			if _menu_cell.y >= 0 and _menu_cell.y < _order.size():
				text = _row_cell(_order[_menu_cell.y], _menu_cell.x)
		1:  # Copy selection
			text = get_selected_text()
		2:  # Copy row
			if _menu_cell.y >= 0 and _menu_cell.y < _order.size():
				var data_row: int = _order[_menu_cell.y]
				var row: PackedStringArray = _rows[data_row] if data_row < _rows.size() else PackedStringArray()
				text = "\t".join(row)
		3:  # Copy column
			if _menu_cell.x >= 0:
				var lines: PackedStringArray = PackedStringArray()
				for i in _order.size():
					lines.append(_row_cell(int(_order[i]), _menu_cell.x))
				text = "\n".join(lines)
		4:  # Copy header
			if _menu_cell.x >= 0 and _menu_cell.x < _headers.size():
				text = str(_headers[_menu_cell.x])
	if not text.is_empty():
		DisplayServer.clipboard_set(text)
