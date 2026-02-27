## 概述

本工程是一个基于 Qt/C++ 的桌面应用，用于导入和编辑 CAN DBC / Excel 信号定义，并支持导出为 DBC/Excel，同时提供可视化界面进行浏览、查询和编辑。整体采用分层架构，核心目标是：

- **数据层解耦界面层**：解析、存储、导出逻辑独立于 UI。
- **统一数据模型**：DBC 与 Excel 共用同一套中间模型。
- **插件式扩展**：便于以后增加其它格式（如 JSON、ARXML 等）。

---

## 总体架构分层

- **UI 层（Presentation Layer）**
  - 主窗口 `MainWindow`
  - 菜单/工具栏（导入、导出、编辑、视图）
  - 信号/报文/节点视图（如 `QTableView`, `QTreeView`）
  - 编辑对话框（新增/修改报文、信号、节点）
- **应用服务层（Application / Service Layer）**
  - 导入服务：`ImportService`
  - 导出服务：`ExportService`
  - 会话管理：`SessionManager`
  - 命令/撤销重做：`CommandManager`
- **领域模型层（Domain Model Layer）**
  - `CanDatabase`（整个 DBC/Excel 的抽象）
  - `CanNode` / `CanMessage` / `CanSignal` / `CanAttribute` 等
- **数据访问与格式适配层（Data Access / Format Adapters）**
  - `DbcParser` / `DbcWriter`
  - `ExcelParser` / `ExcelWriter`
  - 将具体格式转换为领域模型，或从领域模型导出。
- **基础设施层（Infrastructure Layer）**
  - 日志 / 配置 / 最近打开文件记录
  - 文件选择、路径管理
  - 应用启动脚本、打包（如 AppImage）

---

## 模块说明

### UI 层

- **`MainWindow`**
  - 包含菜单栏：`文件`（导入 DBC/Excel，导出 DBC/Excel，最近文件）、`编辑`（撤销/重做、查找）、`视图`（不同面板显示）、`帮助`。
  - 中央区使用 `QTabWidget` 或 `QSplitter` 展示多种视图：
    - 报文列表视图（按 CAN ID 排序）
    - 信号视图（按报文或按节点分组）
    - 节点视图
  - 状态栏显示当前文件名、格式、修改状态、过滤条件等。
- **视图组件**
  - `MessageTableView` / `SignalTableView` / `NodeTreeView` 等派生自 `QTableView` / `QTreeView`。
  - 通过对应的 `QAbstractItemModel` / `QSortFilterProxyModel` 绑定到领域模型。
- **编辑组件**
  - `MessageEditorDialog`：编辑报文 ID、名称、DLC、发送节点等。
  - `SignalEditorDialog`：编辑信号起始位、长度、字节序、缩放、偏移、物理单位、注释等。
  - 支持批量编辑和复制粘贴（可选）。

---

### 应用服务层

- **`SessionManager`**
  - 持有当前打开的 `CanDatabase` 实例。
  - 负责“新建 / 打开 / 关闭 / 保存”生命周期管理。
  - 提供“是否有未保存修改”状态，驱动 UI 提示。
- **`ImportService`**
  - 方法示例：
    - `loadDbc(const QString &filePath) -> CanDatabase`
    - `loadExcel(const QString &filePath) -> CanDatabase`
  - 内部调用各自 Parser，将异常/错误转换为用户可读的提示信息。
- **`ExportService`**
  - 方法示例：
    - `saveAsDbc(const CanDatabase &, const QString &filePath)`
    - `saveAsExcel(const CanDatabase &, const QString &filePath)`
  - 封装 Writer，统一错误处理和确认覆盖逻辑。
- **`CommandManager`（基于命令模式，可选）**
  - 提供撤销/重做支持：
    - `AddMessageCommand`
    - `EditSignalCommand`
    - `DeleteNodeCommand` 等
  - 集中记录修改历史，UI 通过它操作模型。

---

### 领域模型层

- **核心类**
  - `CanDatabase`
    - 成员示例：`QMap<QString, CanNode> m_nodes;`、`QMap<uint32_t, CanMessage> m_messages;`、全局属性、版本信息等。
  - `CanNode`
    - 成员示例：节点名称、注释、发送/接收的报文列表引用等。
  - `CanMessage`
    - 成员示例：ID、名称、DLC、是否扩展帧、发送节点、`QList<CanSignal>` 信号集合、注释等。
  - `CanSignal`
    - 成员示例：起始位、长度、字节序、符号/无符号、因子、偏移、最小/最大值、单位、接收节点、枚举值表等。
  - `CanAttribute` / `CanValueTable`（如需要）
- **特点**
  - 与具体文件格式无关，仅表达语义。
  - 提供查询接口（按报文 ID 查找、按节点查询信号等），减轻 UI 与 Service 的逻辑负担。

---

### 数据访问与格式适配层

- **`DbcParser`**
  - 输入：DBC 文件路径 / 文本流。
  - 输出：填充好的 `CanDatabase`。
  - 负责解析 DBC 语法、报文、信号、节点、属性等。
- **`DbcWriter`**
  - 输入：`CanDatabase`。
  - 输出：符合 DBC 语法的文本写入文件。
- **`ExcelParser`**
  - 输入：Excel 文件路径（约定表头格式，比如：报文名、ID、信号名、起始位、长度、因子、偏移等）。
  - 输出：构造 `CanDatabase`。
  - 需要对列映射进行配置或提供默认模板。
- **`ExcelWriter`**
  - 输入：`CanDatabase`。
  - 输出：Excel 文件（可按报文/信号分 sheet）。
- **错误与日志**
  - 解析层记录详细日志（行号、字段名、错误原因），上传至 UI 弹窗或日志窗口。

---

### 基础设施层

- **配置管理**
  - 最近打开文件列表、默认导出目录、Excel 模板路径等。
- **日志系统**
  - 将导入/导出/解析错误写入日志文件，方便排查问题。
- **启动与打包**
  - 通过 `build.sh`、`run_dbc_viewer.sh` 以及 AppImage/desktop 文件启动应用。
  - 与 Qt 平台插件、图标、桌面集成等打包逻辑。

---

## 主要业务流程

### 导入 DBC/Excel

1. 用户在 `MainWindow` 中点击“导入 DBC”或“导入 Excel”。
2. UI 调用 `ImportService`，后者选择 `DbcParser` 或 `ExcelParser`。
3. 解析完成后生成 `CanDatabase`，交由 `SessionManager` 持有，并通知各视图模型刷新。
4. 视图重新加载数据并展示。

### 导出 DBC/Excel

1. 用户在 `MainWindow` 中点击“导出 DBC”或“导出 Excel”。
2. UI 从 `SessionManager` 获取当前 `CanDatabase`。
3. 调用 `ExportService` 使用 `DbcWriter` 或 `ExcelWriter` 写入目标文件。
4. 结果通过消息框或状态栏反馈。

### 编辑与撤销/重做

1. 用户在表格或树视图中双击项或选择“编辑…”。
2. 弹出相应的编辑对话框，绑定领域对象的拷贝。
3. 用户确认后构造 `Command`（如 `EditSignalCommand`），提交给 `CommandManager`。
4. `CommandManager` 修改 `CanDatabase` 并发出数据变更信号，更新 UI。
5. 撤销/重做操作通过 `CommandManager` 回滚或重做修改。

---

## 扩展与演进方向

- **新格式支持**：为 JSON/ARXML 增加 `JsonParser` / `JsonWriter` 等新适配器，无需改动 UI。
- **高级视图**：增加信号波形预览、差异对比（两个 DBC 文件 diff）。
- **脚本接口**：提供命令行或脚本 API，支持批量转换与检查。
- **校验与规则检查**：在 Service 层增加一致性校验（ID 冲突、信号重名、量纲不一致等）。

