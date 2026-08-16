# VortarisCSV — AI 调试指南 / AI Debugging Guide

面向 **AI / 自动化 / CI** 的调试入口。编辑器 CSV 主窗口（双击单元格编辑）是**给人用**
的交互工具；AI 请用下面的 **MCP `run_script` 直接调 API** 或 **headless CLI**。

This document is aimed at **AI agents / automation / CI**. The editor **CSV**
main screen (double-click to edit cells) is a **human** interaction tool; AI
should use the **MCP `run_script` API snippets** or the **headless CLI** below.

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

---

## 2. Headless CLI — 参数表

独立命令行入口：`demo/scripts/cli_entry.gd`（`extends SceneTree`）。所有
`--vortaris-*` 参数必须放在 `--` 之后（由 `OS.get_cmdline_user_args()` 读取）。
输出统一带 `[vortariscsv]` 前缀，方便 grep / 解析。

`demo/scripts/cli_entry.gd` — a standalone `extends SceneTree` CLI. All
`--vortaris-*` args must come **after `--`** (read via `OS.get_cmdline_user_args()`).
Every output line is prefixed with `[vortariscsv]`.

> **一次性前置步骤 / one-time prerequisite（全新 clone）**
> CLI 依赖 GDExtension（`vortariscsv.gdextension`）。扩展缓存
> `.godot/extension_list.cfg` 被 gitignore，全新 clone 里不存在，此时
> `--script` 模式不会加载扩展，任何 `VCSV*` 类都不可用，CLI 会提示
> `[vortariscsv] ERROR: GDExtension not loaded` 并退出 1。
> 首次运行 CLI 前，先执行一次：
> ```bash
> godot --headless --editor --import --quit --path demo
> ```
> （或在编辑器中打开一次该项目生成缓存）。之后 CLI 即可正常运行。

| 参数 | 作用 | 退出码 |
|---|---|---|
| `--vortaris-csv-validate <file>` | 解析 CSV + 数据完整性校验（打印行/列/表头/告警/问题） | `0` 干净；`1` 解析失败或有校验问题 |
| `--vortaris-csv-stats <file>` | 打印表头、列类型推断、每列统计（non_empty/numeric/distinct） | `0` 成功；`1` 不可恢复错误 |
| 其他 / 未知参数 | 打印用法 | `1` |

### 命令行示例 / examples

```bash
"E:/Godot/Godot_v4.7-stable_win64/Godot_v4.7-stable_win64_console.exe" \
    --headless --path demo \
    --script res://scripts/cli_entry.gd \
    -- --vortaris-csv-validate res://data/monsters.csv

"E:/Godot/Godot_v4.7-stable_win64/Godot_v4.7-stable_win64_console.exe" \
    --headless --path demo \
    --script res://scripts/cli_entry.gd \
    -- --vortaris-csv-stats res://data/monsters.csv
```

### 退出码约定 / exit-code convention

- `0`：成功 / 校验通过。
- `1`：解析失败、校验发现问题、或未知参数。
- 配合 shell 可直接判断：`echo $?`（或 `${PIPESTATUS[0]}`）。

---

## 3. 日志分级 / log levels

- 项目设置 `vortariscsv/general/verbose`（bool，默认 `false`）——在编辑器
  *Project Settings* 里可见；`demo/project.godot` 已写入 `general/verbose=false`。
  0.2.x 的扁平路径 `vortariscsv/verbose` 仍被作为回退读取，老项目无需改动。
- C++ 侧门控（`src/core/vcsv_log.h`）：
  - `log_info`：仅 **debug 构建**（godot-cpp 对 `template_debug`/`editor`
    定义 `DEBUG_ENABLED`，`template_release` 不定义）输出，前缀 `[vortariscsv]`。
  - `log_verbose`：**debug 构建 且** `vortariscsv/general/verbose == true`，前缀
    `[vortariscsv][v]`。
  - 错误/警告（`push_error` / `push_warning`）不受门控影响，各级别都保留。

> 提示：启用 verbose 最简单的方式是临时把 `demo/project.godot` 里的
> `vortariscsv/general/verbose` 改为 `true`，或写一个临时 `extends SceneTree` 脚本在开头
> 调用 `ProjectSettings.set_setting("vortariscsv/general/verbose", true)` 再触发解析。

---

## 4. 常用验证命令 / quick verification

```bash
# 一次性前置步骤（全新 clone）+ 编辑器导入管线检查：
# 生成扩展缓存 .godot/extension_list.cfg（gitignore，全新 clone 不存在），
# 并验证插件加载 + .csv 重新导入。没有它，CLI 会提示 "GDExtension not
# loaded" 而退出 1，回归脚本也会因类缺失而失败。退出 0 即正常。
godot --headless --editor --import --quit --path demo

# 冒烟：应打印 "VortarisCSV demo loaded"，退出 0。
# 注：普通冒烟不会生成扩展缓存；全新 clone 上请先运行上面的 --editor --import。
godot --headless --path demo --quit

# 单套回归（demo/scripts/test_*.gd），退出 0 = 通过
godot --headless --path demo --script res://scripts/test_validation.gd
```
