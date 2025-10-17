#!/bin/bash

# DBC Viewer 启动脚本
# 自动检测和设置GUI环境

echo "DBC Viewer 启动脚本"
echo "=================="

# 检查可执行文件
if [ ! -f "build/DBCViewer" ]; then
    echo "❌ 错误: 找不到可执行文件 build/DBCViewer"
    echo "请先运行 ./build.sh 编译应用程序"
    exit 1
fi

echo "✅ 找到可执行文件"

# 检查DBC文件
if [ ! -f "ADC321_CAN_ADASTORADAR_2025_08_25_V0.0.2.dbc" ]; then
    echo "⚠️  警告: 找不到示例DBC文件"
    echo "您仍然可以手动选择其他DBC文件"
fi

# 检查X服务器
if ! pgrep -x "Xorg" > /dev/null; then
    echo "❌ 错误: 未检测到X服务器运行"
    echo "请确保您运行在图形界面环境中"
    exit 1
fi

echo "✅ 检测到X服务器"

# 设置DISPLAY环境变量
if [ -z "$DISPLAY" ]; then
    echo "⚠️  DISPLAY环境变量未设置，尝试设置为 :0"
    export DISPLAY=:0
fi

echo "✅ DISPLAY设置为: $DISPLAY"

# 检查Qt平台插件
QT_PLUGIN_PATH="/usr/lib/x86_64-linux-gnu/qt5/plugins"
if [ -d "$QT_PLUGIN_PATH" ]; then
    export QT_PLUGIN_PATH="$QT_PLUGIN_PATH"
    echo "✅ 设置Qt插件路径: $QT_PLUGIN_PATH"
fi

# 启动应用程序
echo ""
echo "🚀 启动DBC Viewer..."
echo "=================="

# 启动GUI版本
echo "启动DBC Viewer GUI版本..."
echo "使用说明:"
echo "1. 点击菜单 'File' -> 'Open DBC File...'"
echo "2. 选择DBC文件"
echo "3. 浏览CAN消息和信号"
echo ""

cd build
./DBCViewer

# 检查退出状态
if [ $? -eq 0 ]; then
    echo "✅ 应用程序正常退出"
else
    echo "❌ 应用程序异常退出 (退出码: $?)"
    echo ""
    echo "故障排除建议:"
    echo "1. 确保运行在图形界面环境中"
    echo "2. 检查Qt5运行时库是否完整安装"
    echo "3. 尝试运行: sudo apt install qt5-default"
    echo "4. 如果使用SSH，确保启用了X11转发"
fi
