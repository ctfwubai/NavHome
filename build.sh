#!/usr/bin/env bash
# 导航之家（NavHome）构建脚本
# 需在 MSYS2 / Git Bash 环境运行，依赖 mingw-w64-x86_64-qt6-base、cmake、ninja
set -e

# —— MSYS2 mingw64 工具链路径（如与你本机不一致请修改）——
MINGW="${MINGW:-/c/msys64/mingw64}"
export PATH="$MINGW/bin:$PATH"

# —— 源码目录（支持中文路径）——
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR_WIN="$(cygpath -w "$SRC_DIR")"

# —— 构建目录 ——
# 说明：Qt 的 moc 工具无法写入含中文的输出路径，
#       因此构建产物统一输出到纯 ASCII 目录，源码仍保留在项目目录。
BUILD_DIR="${NAVHOME_BUILD_DIR:-E:/NavHome-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
NAVHOME_CONSOLE="${NAVHOME_CONSOLE:-OFF}"

echo "==> 配置构建：源码 $SRC_DIR_WIN -> 输出 $BUILD_DIR ($BUILD_TYPE)"

"$MINGW/bin/cmake.exe" -S "$SRC_DIR_WIN" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_PREFIX_PATH="$(cygpath -w "$MINGW")" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DNAVHOME_CONSOLE="$NAVHOME_CONSOLE" \
    "$@"

echo "==> 开始编译..."
"$MINGW/bin/cmake.exe" --build "$BUILD_DIR"

echo "==> 构建完成，产物位于：$BUILD_DIR/NavHome.exe"
