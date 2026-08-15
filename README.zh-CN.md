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
- **v0.2.0**：VortarisCSV 成为 `.csv` 默认导入器（并提供一键把存量文件切换过来）；`hp:int` 显式表头 schema；热重载；大数据懒加载；分隔符自动检测；多级表头；`validate()` 数据完整性校验；增量导出；编辑器表格预览停靠面板（双击编辑并回写）
- `compatibility_minimum = "4.7"`（GDExtension 向上兼容）

```gdscript
# 一行读取 CSV 为 Array[Dictionary]（CSVAccess 风格）
var rows: Array = VCSVUtil.load_csv_dict_array("res://data/monsters.csv")

# UE DataTable 风格：绑定到类型化 GDScript 类
var table: VCSVDataTable = VCSVDataTable.from_file(
    "res://data/monsters.csv", null, "res://scripts/row_types/monster_row.gd")
var goblin: MonsterRow = table.get_row("goblin")
print(goblin.health, " ", goblin.position)
```

## 特性

- **低层** —— `VCSVParser.parse_string()` → `VCSVParseResult` → `VCSVTable`（原始字符串网格 + 完整查询 API），`VCSVWriter` 负责序列化。
- **高层** —— `VCSVDataTable`：反射绑定、主键查询、懒构建行对象 + 缓存、行类型脚本变更自动重绑定。
- **编辑器** —— 把 `.csv`/`.tsv` 拖进项目即自动导入为 `VCSVDataTable` 资源（`.tres`）。逐资产可配置；默认覆盖 Godot 内置翻译 CSV 导入器（可在项目设置 `vortariscsv/import/override_translation_importer` 关闭），任意文件也可在导入面板 *Import As* 下拉手动切回翻译导入器。

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
"<godot>/Godot_v4.7-stable_win64_console.exe" --headless --path demo \
      --script res://scripts/test_parser.gd
```

无头回归测试位于 `demo/scripts/`（`extends SceneTree`，退出码 `0`/`1`）。

## 文档

- `docs/quickstart.md` — 5 分钟上手
- `docs/data_types.md` — 支持的类型矩阵、`column_types` 语法、数组/JSON/外键
- `docs/import_pipeline.md` — 编辑器导入流程、`.tres` 存储、优先级覆盖
- `docs/architecture.md` — 三层架构、反射绑定、性能说明

## 许可

MIT — 见 [LICENSE](LICENSE)。
