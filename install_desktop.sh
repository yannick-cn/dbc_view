#!/bin/bash

# DBC Viewer 桌面快捷方式安装脚本

echo "DBC Viewer 桌面快捷方式安装"
echo "=========================="

# 获取当前目录的绝对路径
CURRENT_DIR=$(pwd)
DESKTOP_FILE="$CURRENT_DIR/DBCViewer.desktop"

echo "当前目录: $CURRENT_DIR"
echo "桌面文件: $DESKTOP_FILE"

# 检查可执行文件是否存在
if [ ! -f "$CURRENT_DIR/build/DBCViewer" ]; then
    echo "❌ 错误: 找不到可执行文件 build/DBCViewer"
    echo "请先运行 ./build.sh 编译应用程序"
    exit 1
fi

echo "✅ 找到可执行文件"

# 检查图标文件是否存在
if [ ! -f "$CURRENT_DIR/icon.png" ]; then
    echo "❌ 错误: 找不到图标文件 icon.png"
    exit 1
fi

echo "✅ 找到图标文件"

# 更新桌面文件中的路径
sed -i "s|/home/jingyang/code/dbc_view|$CURRENT_DIR|g" "$DESKTOP_FILE"

echo "✅ 更新桌面文件路径"

# 设置桌面文件权限
chmod +x "$DESKTOP_FILE"
echo "✅ 设置桌面文件权限"

# 复制到用户桌面目录
if [ -d "$HOME/Desktop" ]; then
    cp "$DESKTOP_FILE" "$HOME/Desktop/"
    echo "✅ 复制到桌面目录: $HOME/Desktop/"
fi

# 复制到应用程序目录
if [ -d "$HOME/.local/share/applications" ]; then
    cp "$DESKTOP_FILE" "$HOME/.local/share/applications/"
    echo "✅ 安装到应用程序目录: $HOME/.local/share/applications/"
    
    # 更新桌面数据库
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$HOME/.local/share/applications/"
        echo "✅ 更新桌面数据库"
    fi
fi

# 设置文件关联（可选）
echo ""
echo "🎉 安装完成！"
echo ""
echo "现在您可以："
echo "1. 在桌面上双击 'DBC Viewer' 图标启动应用程序"
echo "2. 在应用程序菜单中找到 'DBC Viewer'"
echo "3. 右键点击 .dbc 文件，选择 '用 DBC Viewer 打开'"
echo ""
echo "如果桌面图标没有出现，请："
echo "1. 刷新桌面 (F5 或右键刷新)"
echo "2. 检查桌面设置是否显示图标"
echo "3. 尝试重新登录"
