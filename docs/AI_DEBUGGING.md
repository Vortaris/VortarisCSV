# VortarisCSV — AI 调试指南 / AI Debugging Guide

面向 **AI / 自动化 / CI** 的调试入口。编辑器 CSV 主窗口（双击单元格编辑）是**给人用**
的交互工具；AI 请用下面的 **MCP `run_script` 直接调 API** 或 **headless CLI**。

This document is aimed at **AI agents / automation / CI**. The editor **CSV**
main screen (double-click to edit cells) is a **human** interaction tool; AI
should use the **MCP `run_script` API snippets** or the **headless CLI** below.

---

## 0. 一次性前置步骤 / one-time prerequisite（全新 clone）

CLI 与 `--script` 回归测试都依赖 GDExtension（`vortariscsv.gdextension`）。
扩展缓存 `.godot/extension_list.cfg` 被 gitignore，全新 clone 里不存在；此时
`--script` 模式不会加载扩展，任何 `VCSV*` 类都不可用。

Both the CLI and the `--script` regression tests depend on the GDExtension.
The extension cache `.godot/extension_list.cfg` is gitignored, so it does not
exist on a fresh clone; until it does, `--script` mode skips
`vortariscsv.gdextension` and every `VCSV*` class is missing.

```bash
godot --headless --editor --import --quit --path demo
```

（或在编辑器中打开一次该项目生成缓存。）之后 CLI 与回归脚本即可正常运行。
(or open the project in the editor once; then the CLI and regression scripts work.)

---

## 1. MCP run_script — 直接调用插件 API

Godot MCP 的 `run_script` 工具执行 `extends RefCounted` 的 GDScript，脚本内可访问
全部 `VCSV*` 类（插件由 `vortariscsv.gdextension` 加载，与运行模式无关）。

The Godot MCP `run_script` tool runs an `extends RefCounted` GDScript with full
access to every `VCSV*` class.

### 1.1 一行读取 CSV（类型化字典行）/ one-liner typed rows

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
	if rows.is_empty():
		return {"ok": false, "reason": "parse failed or empty"}
	return {"ok": true, "row_count": rows.size(), "first_row": rows[0]}
```

失败时（文件不存在、解析错误）返回空数组；先判断 `is_empty()` 再取下标。
On failure (missing file / parse error) it returns an empty `Array`; check
`is_empty()` before indexing.

### 1.2 解析 + 校验 / parse + validate

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var table: VCSVDataTable = VCSVDataTable.from_file(
			"res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
	var issues: PackedStringArray = table.validate({"required_columns": ["id", "name"]})
	return {
		"rows": table.row_count(),
		"cols": table.column_count(),
		"headers": table.headers,
		"issues": issues,
		"errors": table.get_last_errors(),
		"warnings": table.get_last_warnings(),
	}
```

### 1.3 VCSVParseOptions 自定义解析 / custom parse options

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var opts := VCSVParseOptions.new()
	opts.delimiter = ";"
	opts.has_header = true
	opts.encoding = "utf8"
	opts.header_type_separator = ":"     # "hp:int" -> header "hp", type int
	opts.auto_detect_delimiter = false
	opts.max_errors = 50
	var result := VCSVParser.parse_file("res://data/monsters.csv", opts)
	if result == null or not result.success:
		return {"ok": false, "message": result.message if result else "null",
				"line": result.error_line if result else -1, "col": result.error_column if result else -1}
	return {
		"ok": true,
		"headers": result.table.headers,
		"rows": result.table.get_row_count(),
		"warnings": result.warnings,
		"column_types": result.column_types,
	}
```

### 1.4 原始字符串网格 / raw string grid

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var result := VCSVUtil.parse_file("res://data/monsters.csv", null)
	if result == null or not result.success:
		return {"ok": false, "message": result.message if result else "null"}
	return {
		"ok": true,
		"headers": result.table.headers,
		"row_0": result.table.get_row(0),
		"row_dict_0": result.table.get_row_dict(0),
		"row_count": result.table.get_row_count(),
		"col_count": result.table.get_col_count(),
	}
```

### 1.5 列类型推断 / column type inference

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var result := VCSVUtil.parse_file("res://data/monsters.csv", null)
	if result == null or not result.success:
		return {"ok": false}
	return {"ok": true, "types": VCSVUtil.detect_types(result.table, ";", true)}
```

`detect_types(table, array_delimiter, detect_booleans)` 返回 `header → 类型名`
字典（如 `{"health": "int", "attack": "float", "alive": "bool"}`）。

### 1.6 编辑 + 写回 / edit + write back

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var dt: VCSVDataTable = VCSVDataTable.from_file(
			"res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
	dt.set_cell_value("goblin", "health", "999")   # edit the grid, rebuild cache
	var w := VCSVWriter.new()
	w.line_ending = "\n"
	var err := w.write_table(dt.to_table(), "user://patched.csv")
	return {"ok": err == OK, "error": err, "health_now": dt.get_row("goblin").health}
```

### 1.7 类型化一次性加载（0.3.1）/ typed one-shot (0.3.1)

```gdscript
extends RefCounted

func execute(scene_tree: SceneTree) -> Variant:
	var t := VCSVDataTable.load_typed("res://data/monsters.csv",
			"res://scripts/row_types/monster_row.gd")
	if t == null:
		return {"ok": false, "reason": "load_typed failed"}
	var goblin = t.get_row("goblin")                # cache hit after first call
	var tags: Array = t.get_field_array("goblin", "tags")   # native Array
	return {"ok": true, "name": goblin.name, "health": goblin.health, "tags": tags}
```

---

## 2. Headless CLI — 参数表

独立命令行入口：`demo/scripts/cli_entry.gd`（`extends SceneTree`）。所有
`--vortaris-*` 参数必须放在 `--` 之后（由 `OS.get_cmdline_user_args()` 读取）。
输出统一带 `[vortariscsv]` 前缀，方便 grep / 解析。

`demo/scripts/cli_entry.gd` — a standalone `extends SceneTree` CLI. All
`--vortaris-*` args must come **after `--`** (read via `OS.get_cmdline_user_args()`).
Every output line is prefixed with `[vortariscsv]`.

> **前置步骤见第 0 节** / see section 0 for the one-time prerequisite.
> If the extension cache is missing, the CLI prints
> `[vortariscsv] ERROR: GDExtension not loaded` and exits 1.

| 参数 | 作用 | 退出码 |
|---|---|---|
| `--vortaris-csv-validate <file>` | 解析 CSV + 数据完整性校验（打印行/列/表头/告警/问题） | `0` 干净；`1` 解析失败或有校验问题 |
| `--vortaris-csv-stats <file>` | 打印表头、列类型推断、每列统计（non_empty/numeric/distinct） | `0` 成功；`1` 不可恢复错误 |
| 其他 / 未知参数 | 打印用法 | `1` |

### 命令行示例 / examples

```bash
godot --headless --path demo \
    --script res://scripts/cli_entry.gd \
    -- --vortaris-csv-validate res://data/monsters.csv

godot --headless --path demo \
    --script res://scripts/cli_entry.gd \
    -- --vortaris-csv-stats res://data/monsters.csv
```

Sample `--vortaris-csv-validate` output (exit 0):

```
[vortariscsv] validate res://data/monsters.csv
[vortariscsv]   rows: 2
[vortariscsv]   columns: 9
[vortariscsv]   headers: ["id", "name", "health", ...]
[vortariscsv]   parse warnings: 0
[vortariscsv]   validation issues: 0
[vortariscsv] validate OK
```

`--vortaris-csv-stats` additionally prints, per column, the inferred type and
`non_empty / numeric / distinct`.

### 退出码约定 / exit-code convention

- `0`：成功 / 校验通过。
- `1`：解析失败、校验发现问题、或未知参数。
- 配合 shell 可直接判断：`echo $?`（或 `${PIPESTATUS[0]}`）。

---

## 3. 日志分级 / log levels

- 项目设置 `vortariscsv/general/verbose`（bool，默认 `false`）——在编辑器
  *Project Settings* 里可见；`demo/project.godot` 已写入 `general/verbose=false`。
  0.2.x 的扁平路径 `vortariscsv/verbose` 会在编辑器启动时迁到新路径并被移除（避免
  Project Settings 出现两个 verbose）；C++ 侧仍会回退读取扁平路径，以兼容尚未在
  编辑器中打开过的老项目。
- C++ 侧门控（`src/core/vcsv_log.h`）：
  - `log_info`：仅 **debug 构建**（godot-cpp 对 `template_debug`/`editor`
    定义 `DEBUG_ENABLED`，`template_release` 不定义）输出，前缀 `[vortariscsv]`。
  - `log_verbose`：**debug 构建 且** `vortariscsv/general/verbose == true`，前缀
    `[vortariscsv][v]`。
  - 错误/警告（`push_error` / `push_warning`）不受门控影响，各级别都保留。

> 提示：启用 verbose 最简单的方式是临时把 `demo/project.godot` 里的
> `vortariscsv/general/verbose` 改为 `true`，或写一个临时 `extends SceneTree` 脚本在开头
> 调用 `ProjectSettings.set_setting("vortariscsv/general/verbose", true)` 再触发解析。
> Errors and warnings are **not** gated — they always print.

---

## 4. 常见排查 / common failure patterns

| 症状 / Symptom | 原因 / Cause | 处理 / Fix |
|---|---|---|
| CLI 打印 `GDExtension not loaded` 并退出 1 | 全新 clone 缺少 `.godot/extension_list.cfg` | 运行 `godot --headless --editor --import --quit --path demo` 一次 |
| `--script` 回归测试报 "Identifier not declared: VCSVParser" | 同上 | 同上 |
| `get_row()` 返回 `null` | 键不在 `key_column`，或 `row_type` 为空/无法实例化 | 检查 `table.get_last_errors()`；确认 `row_type` 是 `extends Resource` 脚本路径 |
| 校验报 "missing required column" | 列不存在或表头拼写不一致 | 核对 `headers` 与 `required_columns`；注意大小写 |
| 校验报 "duplicate key" | `key_column` 有重复值 | 修复 CSV；或按调用传 `{"check_duplicate_keys": false}` |
| 校验报 "unresolved foreign key" | OBJECT 列的值在 `linked_tables` 目标表里没有对应键 | 检查 `linked_tables` 路径与目标表行类型类名 |
| 中文乱码 | 文件是 GBK/GB2312 编码 | `opts.encoding = "gbk"`（或导入面板 `encoding` 选项） |

---

## 5. 常用验证命令 / quick verification

```bash
# 一次性前置步骤（全新 clone）+ 编辑器导入管线检查：
# 生成扩展缓存 .godot/extension_list.cfg（gitignore，全新 clone 不存在），
# 并验证插件加载 + .csv 重新导入。没有它，CLI 会提示 "GDExtension not
# loaded" 而退出 1，回归脚本也会因类缺失而失败。退出 0 即正常。
godot --headless --editor --import --quit --path demo

# 冒烟：应打印 "VortarisCSV demo loaded"，退出 0。
godot --headless --path demo --quit

# 单套回归（demo/scripts/test_*.gd），退出 0 = 通过
godot --headless --path demo --script res://scripts/test_validation.gd
godot --headless --path demo --script res://scripts/test_parser.gd
godot --headless --path demo --script res://scripts/test_datatable_script.gd

# 性能冒烟（软时间目标；~100 万单元格）
godot --headless --path demo --script res://scripts/perf_test.gd
```
