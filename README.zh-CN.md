# VortarisCSV

[English](README.md) | **简体中文**

> **API 参考：** 权威的逐类 API 文档在 `doc_classes/*.xml`（编译进扩展，编辑器里按 `F1` 即可查看）。本 README 只是概览；如与 `doc_classes/` 不一致，以 XML 为准。

面向 **Godot 4.7** 的现代 CSV/DSV 数据处理插件，使用 **C++ (GDExtension / godot-cpp)** 编写。

**易上手、又灵活**：吸收了三个流行 GDScript CSV 插件的精华，用原生 C++ 核心修复了它们的缺陷：

- RFC 4180 状态机解析器（`char32_t` 指针单遍遍历，零逐字符分配）
- **UE DataTable 式反射绑定**：把 CSV 行直接绑定到自定义行类型——GDScript `extends Resource` 类 或 C++ `ClassDB` 注册类，列名自动映射到 `@export` 属性
- 双层 API：快速字符串层（`VCSVParser` / `VCSVTable` / `VCSVWriter`）+ 高层对象层（`VCSVDataTable`），外加完整编辑器导入管线（`.csv`/`.tsv` → `.tres`）
- 类型化单元格：标量、枚举、`Vector2/3/4`、`Color`、`Rect2`、变换、数组（子分隔符）、JSON 单元格、跨表懒引用（外键）
- 结构化错误（行列号）、BOM 处理、注释行、可配置分隔符/引号、CSV 注入防护导出
- **v0.2.0**：VortarisCSV 成为 `.csv` 默认导入器（并提供一键把存量文件切换过来）；数组单元格同时支持 `;` 分隔与 JSON 数组字面量两种形式；`hp:int` 显式表头 schema；`VCSVUtil.load_csv_dict()` 单行读取与 `VCSVDataTable.get_table()` 别名；热重载；大数据懒加载；自定义导入分隔符；分隔符自动检测；多级表头；`validate()` 数据完整性校验；增量导出（`export_rows_to_csv` / `export_row_to_csv`）；编辑器表格预览停靠面板（双击编辑并回写）
- **v0.2.1**：面向 AI/CI 的 headless CLI（`res://scripts/cli_entry.gd` → `--vortaris-csv-validate` / `--vortaris-csv-stats`）、分级日志（`vortariscsv/verbose`）、AI 调试指南（`docs/AI_DEBUGGING.md`）、编辑器预览面板默认隐藏 + 手动调出（并修复回写重新导入的报错）
- **v0.3.0**：CSV 编辑器升级为**主窗口工作区**（与 2D/3D/Script 并排的 "CSV" 标签页）——可编辑且列宽可拖拽的数据表、双击单元格编辑并回写源 `.csv`、Import CSV / Export CSV / Export Rows、详情面板（行数/列数/表头/类型推断/校验问题）与状态栏。在 FileSystem 面板双击一个由 Vortaris 导入的 `.csv` 会切到 CSV 标签页并打开（单击只选中、不切屏；可用 `vortariscsv/editor/auto_switch_to_csv` 关闭双击切屏）。修复第一列表头不显示的问题。`vortariscsv/*` 项目设置重构为层级结构（`general` / `import` / `editor` / `validation`）并新增多项设置（见下方「项目设置」）。
- **v0.3.1**：运行时热路径体验优化 —— `VCSVDataTable.load_typed(path, row_type)` / `VCSVUtil.load_csv_typed(path, options, row_type)` **一次性**解析 CSV 并绑定行类型（缓存该表后，重复 `get_row()` 命中内置类型化行缓存，无需每次查找都重建表）；`VCSVDataTable.get_field_array(key, field)` 直接返回数组列的原生 `Array`（无需手工 `split(";")`）；并修复 `Array[String]` 列经 `load_csv_dict_array` / `load_csv_dict` 往返时不再正确的问题（此前会以 `;` 连接的原生字符串返回）。
- **v0.4.0**：CSV 主屏表格换成 **Excel 风格自绘网格**（`VCSVGrid`）——真实单元格边框、行号栏、虚拟滚动（超大表也流畅）、点表头排序、拖动调列宽、单元格/矩形选区 + Ctrl/Cmd+C 与右键复制、双击编辑。单元格编辑回写改为原子写（临时文件 + 重命名），并用 `update_file()` 触发重导入（不再报进度对话框错误）。strict 解析支持错误预算（`VCSVParseOptions.max_errors`）；`VCSVWriter.encoding` 新增 `utf8_bom` / `gbk` 输出（与 GBK 解析对称，GBK 文件可往返）。解析/导入日志受 `vortariscsv/general/verbose` 门控。
- `compatibility_minimum = "4.7"`（GDExtension 向上兼容）

## 快速上手

从「轻」到「重」的三种读法：

```gdscript
# 1) 一行读取 CSV 为 Array[Dictionary]（CSVAccess 风格），每列类型自动推断
#    （int/float/bool/Vector2/Color/Array/...）。
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")
var goblin: Dictionary = rows[0]
print(goblin.health, " ", goblin.position)     # 100  (10, 20)
var first: Dictionary = VCSVUtil.load_csv_dict("res://data/monsters.csv")  # 单行版

# 2) 低层：解析为原始字符串网格（快、不做类型转换）。
var result := VCSVParser.parse_file("res://data/monsters.csv", null)
if result.success:
    var table: VCSVTable = result.table
    print(table.headers, " ", table.get_row(0))   # ["id",...]  ["goblin", ...]

# 3) UE DataTable 风格：绑定到类型化 GDScript 类。
var table: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
var monster: MonsterRow = table.get_row("goblin")
print(monster.health, " ", monster.position)   # 100  (10, 20)

# 4) 0.3.1：类型化一次性加载 —— 缓存该表，get_row() 即缓存命中。
var typed: VCSVDataTable = VCSVDataTable.load_typed(
    "res://data/monsters.csv", "res://scripts/row_types/monster_row.gd")
var tags: Array = typed.get_field_array("goblin", "tags")   # 原生 Array，无需 ";".split
```

完整的 5 分钟引导见 `docs/quickstart.md`；类型矩阵见 `docs/data_types.md`。

## API 总览

| 类 | 层 | 用途 |
|---|---|---|
| `VCSVParser` | 低层 | 静态 `parse_string` / `parse_file` → RFC 4180 网格。不做类型转换。 |
| `VCSVParseOptions` | 低层 | 解析配置：分隔符、引号、注释、严格性、BOM、编码、切片、自动检测、多级表头、表头 schema。 |
| `VCSVParseResult` | 低层 | 结构化结果：`success`、`error`、`message`、`error_line/column`、`warnings`、`table`、`column_types`。 |
| `VCSVTable` | 低层 | 字符串网格容器：单元格/行/列访问、排序/查找、统计、JSON/字典互转、编辑。 |
| `VCSVWriter` | 低层 | 把表/行/字典序列化回 CSV（RFC 4180 引号、行尾、公式注入防护）。 |
| `VCSVDataTable` | 高层 | UE DataTable 风格类型化表：反射绑定、按键查找、懒构建、热重载、校验、外键、增量导出。 |
| `VCSVUtil` | 高层 | 静态一行式：`load_csv_dict_array`、`load_csv_dict`、`load_csv_typed`、`detect_types`、`table_to_dict_array`、`type_name`。 |
| `VCSVEditorImportPlugin` | 编辑器 | C++ `EditorImportPlugin`：`.csv`/`.tsv` → `.tres` `VCSVDataTable`。 |

选择原则：

- **一次性读取、快速查看** → `VCSVUtil.load_csv_dict_array(path)`。
- **需要每行一个类型化对象、按键查找或编辑** → `VCSVDataTable.from_file(path, null, row_type)`（0.3.1 可用 `load_typed`）。
- **需要原始字符串单元格、在网格上搜索/排序或自定义解析** → `VCSVParser` + `VCSVTable`。
- **写 CSV** → `VCSVWriter`。

## 特性

- **低层** —— `VCSVParser.parse_string()` → `VCSVParseResult` → `VCSVTable`（原始字符串网格 + 完整查询 API），`VCSVWriter` 负责序列化。
- **高层** —— `VCSVDataTable`：反射绑定、主键查询、懒构建行对象 + 缓存、行类型脚本变更自动重绑定。
- **编辑器** —— 把 `.csv`/`.tsv` 拖进项目即自动导入为 `VCSVDataTable` 资源（`.tres`）。逐资产可配置；默认覆盖 Godot 内置翻译 CSV 导入器（可在项目设置 `vortariscsv/import/override_translation_importer` 关闭），任意文件也可在导入面板 *Import As* 下拉手动切回翻译导入器。
- **编辑器主窗口** —— "CSV" 标签页（与 2D/3D/Script 并列）提供完整表格编辑：列宽可拖拽、双击单元格编辑并回写源 `.csv`、Import CSV / Export CSV / Export Rows，以及详情面板（行数/列数/表头/推断类型/校验问题）。在 FileSystem 面板双击一个由 Vortaris 导入的 `.csv` 会切到 CSV 标签页并打开；单击只选中、不切屏。将 `vortariscsv/editor/auto_switch_to_csv` 设为 `false` 可连双击也不切屏。

## 使用编辑器 CSV 主窗口

1. **启用插件**：在 Godot 编辑器打开项目，*Project → Project Settings → Plugins* 里启用 **VortarisCSV**。（运行时本身由 `vortariscsv.gdextension` 加载，与此开关无关。）
2. **放入 CSV**：把 `data/monsters.csv` 放进 `res://`。Vortaris 导入器自动接管 `.csv`/`.tsv`（优先级 2.0）并导入为 `VCSVDataTable` 资源。
3. **在编辑器中打开**：在 FileSystem 面板双击该 `.csv`，编辑器切到 **CSV** 标签页并渲染表格。单击只选中文件，不会打断你当前的编辑。
4. **编辑单元格**：双击任意单元格输入并回车。改动会经 `VCSVWriter` 回写源 `.csv`，并在下一帧自动重新导入，`.tres` 保持同步。
5. **调整列宽**：拖动表头分隔线。
6. **导入/导出**：工具栏的 **Import CSV**（打开任意文件）、**Export CSV**（序列化整张表）、**Export Rows**（只写选中的行——Ctrl/Cmd 点击多选）。
7. **查看**：右侧 **Details** 面板显示行列数、表头、推断类型与校验问题（行宽不一致、空/重复表头，以及 `VCSVDataTable.validate()` 的发现）。
8. **设置行类型**：选中 `.csv`，打开 **Import** 面板，把 *Row Type* 设为类似 `res://scripts/row_types/monster_row.gd` 的脚本并重新导入。之后 `load("res://data/monsters.csv").get_row("goblin")` 就返回类型化的 `MonsterRow`。

提示：
- 编辑器为响应性把视图上限设为 1000 行；提示行会告诉你其余数据仍保留并在编辑时重新保存。完整数据始终在文件里。
- 数据表字号由 `vortariscsv/editor/table_font_size`（默认 14）控制。
- `vortariscsv/editor/auto_switch_to_csv`（默认 `true`）控制双击 Vortaris 导入的 `.csv` 是否切到 CSV 标签页。

## 数据驱动集成

编辑器导入的 `.tres` 就是一个 `VCSVDataTable`，在任何场景或 autoload 里都能 `load()`：

```gdscript
# 编辑器导入资源：直接 load 那个 .csv 路径（会解析到 .tres）。
var table: VCSVDataTable = load("res://data/monsters.csv")
var goblin: MonsterRow = table.get_row("goblin")

# 纯运行时（不依赖编辑器）：
var table := VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
```

行类型就是普通的 `extends Resource` 类：

```gdscript
class_name MonsterRow
extends Resource

@export var id: String = ""
@export var name: String = ""
@export var health: int = 0
@export var attack: float = 0.0
@export var alive: bool = false
@export var position: Vector2 = Vector2.ZERO
@export var color: Color = Color.WHITE
@export var tags: Array[int] = []
@export var notes: Dictionary = {}
```

`.tres` 里只存字符串网格 + 配置；类型化行对象在首次访问时按**当前** `row_type` 脚本重建。因此：

- **脚本热重载自动重绑定** —— 改 `MonsterRow.gd` 并保存，下次 `get_row()` 就用新属性，无需重新导入。
- **给 CSV 加一列** —— 要么给行类型加一个 `@export` 属性，要么留作多余列（没有对应属性的列会产生一条警告）。

### 跨表引用（外键）

```gdscript
# WeaponRow 有 `@export var owner: MonsterRow`。
# weapons.linked_tables = {"monsters": "res://data/monsters.csv"}
var weapons: VCSVDataTable = load("res://data/weapons.csv")
var sword: WeaponRow = weapons.get_row("w1")
print(sword.owner.name)                    # 外键懒解析为 MonsterRow
var joined: Array = weapons.join_rows("monsters")   # 扁平字典，"monsters." 前缀
```

> **刷新注意：** 类型化行会在构建时缓存解析到的外键对象。如果你修改了*被关联*的表，也要对引用它的表调用 `refresh()`，以便重建缓存的外键引用。

### 热重载

导入的表会把源 `.csv` 记到 `source_path`。当 `vortariscsv/general/hot_reload_default` 开启（或逐资产 `hot_reload` 选项打开）时，编辑器插件会在文件系统变化时轮询已注册的表，源文件 mtime 变化即重新解析。运行时也能手动驱动同一机制：

```gdscript
var t := VCSVDataTable.load_typed("user://data.csv", "res://scripts/row_types/monster_row.gd")
t.source_path = "user://data.csv"
t.hot_reload = true
if t.poll_hot_reload():
    print("file changed, cache marked dirty")
```

## 项目设置

`vortariscsv/*` 设置按四类分组，在 *Project → Project Settings* 里各自成类。完整参考（路径、默认值、说明）：

| 设置 | 默认值 | 说明 |
|---|---|---|
| `vortariscsv/general/verbose` | `false` | 分级日志开关。0.2.x 的扁平路径 `vortariscsv/verbose` 会在编辑器启动时迁到本路径（旧值复制过去，随后移除扁平键，确保 Project Settings 里只显示一个 `verbose`）；C++ 侧读取仍回退到扁平路径，以兼容尚未在编辑器中打开过的老项目。 |
| `vortariscsv/general/lazy_build_default` | `false` | 新建表格 `lazy_build` 的默认值（作用于 `VCSVDataTable.from_file()` 与编辑器导入）。 |
| `vortariscsv/general/hot_reload_default` | `false` | 新建表格 `hot_reload` 的默认值（源 `.csv` 变更时自动重新导入 `.tres`）。 |
| `vortariscsv/import/override_translation_importer` | `true` | 默认让 Vortaris 导入器接管 `.csv`/`.tsv`（导入优先级 `2.0`）；关闭则恢复 Godot 内置翻译 CSV 导入器为默认。 |
| `vortariscsv/import/delimiter` | `,` | 编辑器导入默认分隔符（如 `,` `;` tab `\|`）。 |
| `vortariscsv/import/encoding` | `utf8` | 编辑器导入默认编码（`utf8`、`gbk`、`gb2312`）。 |
| `vortariscsv/import/auto_detect_delimiter` | `false` | 导入时自动探测分隔符，而不是使用 `delimiter` 默认值。 |
| `vortariscsv/import/header_rows` | `1` | 导入 CSV 的前置表头行数。 |
| `vortariscsv/editor/table_font_size` | `14` | CSV 主窗口数据表字号。 |
| `vortariscsv/editor/auto_switch_to_csv` | `true` | 在 FileSystem 面板双击由 Vortaris 导入的 `.csv` 时切到 CSV 主窗口。设为 `false` 则只在 CSV 标签页已激活时才打开文件。 |
| `vortariscsv/validation/check_duplicate_keys` | `true` | 默认让 `VCSVDataTable.validate()` 报告重复主键值。 |
| `vortariscsv/validation/check_required_columns` | `true` | 默认让 `VCSVDataTable.validate()` 报告缺失的 `required_columns`。 |

说明：
- 逐资产的导入面板覆盖仍然优先于 `vortariscsv/import/*` 默认值。
- 调用时在 options 字典里显式传 `check_duplicate_keys` / `check_required_columns` 可逐次覆盖 `vortariscsv/validation/*` 项目设置。
- **悬停提示**：Godot 4.7 的 `ProjectSettings.add_property_info()` 没有 `description`/tooltip 键 —— 内置设置的说明是编译进编辑器二进制的，自定义设置无法提供悬停说明。上表即权威说明。`hint_string` 只用于其真正的语义（枚举选项 / 数值范围 / 占位文本），绝不塞入自由文本说明，否则会破坏对应提示（参照 ModLoader 的 "Cannot get class" 教训）。

## 常见问题与注意

- **`get_row()` 返回 `null`** —— 键不在 `key_column`，或 `row_type` 为空/无法实例化。用 `get_last_errors()` 查看原因。
- **单元格转换失败** —— 行仍会构建，属性保留默认值，失败记入 `get_last_errors()`（带行列）。
- **没有对应属性的列** —— 被忽略并记录一条警告（`get_last_warnings()`）；没有对应列的属性保留默认值。
- **多字符分隔符会被拒绝** —— `delimiter` 必须是单码点（`","`、`";"`、`"\t"`、`"|"` 等）。多字符分隔符是解析错误。
- **GBK 文件** —— 设置 `VCSVParseOptions.encoding = "gbk"`（或 `"gb2312"`），或使用导入面板的 `encoding` 选项。GBK/GB2312 解码覆盖 GB2312 核心 + 常见 GBK 字符；未知字节变为 U+FFFD。
- **导入的 `.tres` 过期** —— 插件在变更时自动重导入；开启热重载后会自动重新解析。也可在 FileSystem 面板手动强制重导入。
- **`Array[String]` 列** —— 从 v0.3.1 起，经 `load_csv_dict_array` / `load_csv_dict` 与 `get_field_array(key, field)` 都以原生 `Array` 往返，无需手工 `split(";")`。
- **大文件性能** —— 用 `VCSVParseOptions.row_offset` / `max_rows` 切片解析，`lazy_build = true` 把类型化行的构建推迟到真正请求时。

## 构建

前置：安装 [SCons](https://scons.org/)、C++ 编译器（MSVC / MinGW / Clang）、以及一份匹配你的 Godot 版本的
[godot-cpp](https://github.com/godotengine/godot-cpp)（v10 `master` 自带 4.7 的 `extension_api.json`）。

```sh
scons platform=windows target=template_debug arch=x86_64 \
      godot_cpp_path=<path-to-godot-cpp> build_library=False
```

产物：`demo/addons/vortariscsv/bin/vortariscsv.windows.template_debug.x86_64.dll`
（按需替换 `platform`/`target`/`arch`，参见 `docs/cross_platform.md`）。

## 测试

```sh
godot --headless --path demo --script res://scripts/test_parser.gd
```

无头回归测试位于 `demo/scripts/`（`extends SceneTree`，退出码 `0`/`1`）：

- `test_parser.gd` —— RFC 4180 边界（引号、换行、行尾、BOM、注释、严格性、自动检测、多级表头）
- `test_writer.gd` —— 引号规则、行尾、全引号、公式消毒、往返、子集导出
- `test_types.gd` —— 类型推断、类型化字典、JSON/Color、表头 schema
- `test_datatable_script.gd` —— GDScript 行绑定、查找、错误/警告、类型化数组、load_typed 缓存、热重载
- `test_datatable_cpp.gd` —— C++ `ClassDB` 行类型绑定、非法行类型
- `test_import.gd` —— 导入管线（解析 → `.tres` → 重载）、自定义分隔符
- `test_aux.gd` —— 排序/查找/过滤、编辑、JSON 互转
- `test_features.gd` —— GBK、统计、批量 API、外键 join
- `test_validation.gd` —— 必填列、转换错误、重复键、未解析外键
- `test_settings.gd` —— 项目设置默认值与覆盖
- `test_editor_gui.gd` —— CSV 主窗口数据路径（解析/填充、编辑回写、导出）
- `perf_test.gd` —— 约 100 万单元格解析/绑定的软时间冒烟

CLI 入口同样方式运行（见 `docs/AI_DEBUGGING.md`）：

```sh
godot --headless --path demo --script res://scripts/cli_entry.gd -- \
      --vortaris-csv-validate res://data/monsters.csv
```

## 文档

- `docs/quickstart.md` — 5 分钟上手
- `docs/data_types.md` — 支持的类型矩阵、`column_types` 语法、数组/JSON/外键
- `docs/import_pipeline.md` — 编辑器导入流程、`.tres` 存储、优先级覆盖
- `docs/architecture.md` — 三层架构、反射绑定、性能说明
- `docs/cross_platform.md` — Windows/Linux/macOS/移动/Web 构建
- `docs/AI_DEBUGGING.md` — AI / headless CLI 调试指南：MCP `run_script` API 示例、CLI 参数与退出码、日志分级

## 许可

MIT — 见 [LICENSE](LICENSE)。
