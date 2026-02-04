# DBC Viewer

一个基于Qt C++的DBC（Database CAN）文件查看器上位机应用程序。

## 功能特性

- **DBC文件解析**: 完整解析DBC文件格式，支持CAN消息和信号定义
- **树形视图**: 以树形结构显示CAN消息和信号层次关系
- **信号表格**: 详细显示信号属性（起始位、长度、因子、偏移量等）
- **信号详情**: 显示信号的详细属性和值表
- **多标签页**: 分别显示信号属性和值表信息
- **现代UI**: 使用Qt6构建的现代化用户界面

## 系统要求

- **跨平台支持**：Linux、Windows、macOS
- Qt5 或 Qt6 开发环境
- CMake 3.16 或更高版本
- C++17 编译器

## 安装依赖

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential
# 或使用 Qt5: sudo apt install qt5-default cmake build-essential
```

### CentOS/RHEL
```bash
sudo yum install qt6-qtbase-devel qt6-qttools-devel cmake gcc-c++
```

### Windows
1. 安装 [CMake](https://cmake.org/download/)
2. 安装 [Qt](https://www.qt.io/download)（选择 MSVC 或 MinGW 套件）
3. 安装 Visual Studio（若使用 MSVC）或 MinGW（若使用 MinGW 套件）

## 编译和运行

### Linux / macOS

1. 克隆或下载项目到本地
2. 进入项目目录：
   ```bash
   cd dbc_view
   ```

3. 运行构建脚本：
   ```bash
   ./build.sh
   ```

4. 运行应用程序：

   **方式一：使用启动脚本（推荐）**
   ```bash
   ./run_dbc_viewer.sh
   ```

   **方式二：直接运行**
   ```bash
   cd build
   ./DBCViewer
   ```

### Windows

1. 进入项目目录，双击运行 `build.bat`，或在命令提示符中：
   ```cmd
   build.bat
   ```

2. 若 Qt 未安装在默认路径，请先设置环境变量：
   ```cmd
   set QT_DIR=C:\Qt\5.15.2\msvc2019_64
   build.bat
   ```
   （路径请根据实际 Qt 安装位置修改）

3. 运行应用程序：
   ```cmd
   run_dbc_viewer.bat
   ```
   或直接双击 `build\Release\DBCViewer.exe`（或 `build\DBCViewer.exe`）

4. **首次运行**：若提示缺少 Qt DLL，请将 Qt 的 `bin` 目录加入系统 PATH，或使用 `windeployqt` 部署：
   ```cmd
   %QT_DIR%\bin\windeployqt build\Release\DBCViewer.exe
   ```

## 使用方法

1. **打开DBC文件**: 点击菜单 "File" -> "Open DBC File..." 选择要查看的DBC文件
2. **查看消息**: 在左侧树形视图中选择CAN消息，右侧会显示该消息的所有信号
3. **查看信号详情**: 在信号表格中选择特定信号，下方会显示该信号的详细属性和值表
4. **搜索和排序**: 支持对消息和信号进行排序和搜索

## 支持的DBC格式

- CAN消息定义 (BO_)
- 信号定义 (SG_)
- 值表定义 (VAL_)
- 属性定义 (BA_)
- 消息周期时间
- 帧格式信息

## 项目结构

```
dbc_view/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── mainwindow.h/cpp      # 主窗口类
│   ├── dbcparser.h/cpp       # DBC文件解析器
│   ├── canmessage.h/cpp      # CAN消息数据模型
│   └── cansignal.h/cpp       # CAN信号数据模型
├── CMakeLists.txt            # CMake构建配置
├── build.sh                  # 构建脚本
└── README.md                 # 说明文档
```

## 开发说明

项目使用现代C++17标准和Qt6框架开发，采用MVC架构模式：

- **Model**: `CanMessage`和`CanSignal`类提供数据模型
- **View**: `MainWindow`类提供用户界面
- **Controller**: `DbcParser`类处理文件解析逻辑

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。
