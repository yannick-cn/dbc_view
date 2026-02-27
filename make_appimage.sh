#!/bin/bash

# DBC Viewer AppImage build script
set -e

APP_NAME="DBCViewer"
BUILD_DIR="build"
OUTPUT_APPIMAGE=""

echo "=== Building AppImage for ${APP_NAME} ==="

if ! command -v wget >/dev/null 2>&1 && ! command -v curl >/dev/null 2>&1; then
    echo "Error: wget 或 curl 未安装，请先安装其中一个再重试。"
    exit 1
fi

if [ ! -x "${BUILD_DIR}/${APP_NAME}" ]; then
    echo "未找到可执行文件 ${BUILD_DIR}/${APP_NAME}，先执行 ./build.sh..."
    ./build.sh
fi

LINUXDEPLOYQT_APPIMAGE="${LINUXDEPLOYQT_APPIMAGE:-linuxdeployqt-continuous-x86_64.AppImage}"

if [ ! -f "${LINUXDEPLOYQT_APPIMAGE}" ]; then
    echo "未找到 ${LINUXDEPLOYQT_APPIMAGE}，尝试从官方仓库下载..."
    URL="https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
    if command -v wget >/dev/null 2>&1; then
        wget -O "${LINUXDEPLOYQT_APPIMAGE}" "${URL}"
    else
        curl -L -o "${LINUXDEPLOYQT_APPIMAGE}" "${URL}"
    fi
    chmod +x "${LINUXDEPLOYQT_APPIMAGE}"
fi

chmod +x "${LINUXDEPLOYQT_APPIMAGE}"

echo "使用 linuxdeployqt 打包 Qt 依赖..."

DESKTOP_FILE="DBCViewer_appimage.desktop"

if [ ! -f "${DESKTOP_FILE}" ]; then
    echo "Error: 未找到 ${DESKTOP_FILE}，请确认它位于项目根目录。"
    exit 1
fi

APP_ICON_SRC=""
if [ -f "icon.png" ]; then
    APP_ICON_SRC="icon.png"
elif [ -f "icon.svg" ]; then
    APP_ICON_SRC="icon.svg"
fi

if [ -n "${APP_ICON_SRC}" ]; then
    echo "复制图标 ${APP_ICON_SRC} 到 ${BUILD_DIR}/ 供 AppImage 使用..."
    cp "${APP_ICON_SRC}" "${BUILD_DIR}/${APP_ICON_SRC}"
else
    echo "警告：未找到 icon.png 或 icon.svg，将使用默认图标。"
fi

QMAKE_BIN="${QMAKE_BIN:-$(command -v qmake6 || command -v qmake || true)}"

if [ -z "${QMAKE_BIN}" ]; then
    echo "警告：未找到 qmake/qmake6，将让 linuxdeployqt 自动检测 Qt。"
    QMAKE_ARG=""
else
    QMAKE_ARG="-qmake=${QMAKE_BIN}"
fi

./"${LINUXDEPLOYQT_APPIMAGE}" "${DESKTOP_FILE}" ${QMAKE_ARG} -appimage

OUTPUT_APPIMAGE=$(ls -t ./*.AppImage 2>/dev/null | head -n 1 || true)

if [ -z "${OUTPUT_APPIMAGE}" ]; then
    echo "未找到生成的 AppImage 文件，请检查 linuxdeployqt 输出。"
    exit 1
fi

echo "=== AppImage 生成成功 ==="
echo "文件: ${OUTPUT_APPIMAGE}"
echo
echo "在目标机器上运行示例："
echo "chmod +x ${OUTPUT_APPIMAGE}"
echo "./${OUTPUT_APPIMAGE}"

