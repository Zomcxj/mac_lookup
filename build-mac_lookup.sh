#!/bin/bash
# ==============================================================================
#  MAC ARP & WiFi Scanner - 统一构建脚本
#  支持: Windows (Git Bash / MSYS2 / Cygwin) / Linux / ARM
#  用法: ./build.sh [options]
#  选项:
#    --clean       : 清理后重新构建（默认）
#    --no-clean    : 使用缓存，加快重复构建
#    fast          : 快速构建 = --no-clean
#    debug         : Debug 构建
#    release       : Release 构建（默认）
#
#  示例:
#    ./build.sh              # Release 完整构建
#    ./build.sh fast         # 快速构建（日常开发）
#    ./build.sh debug        # Debug 构建
# ==============================================================================

set -e

# ==================== 参数解析 ====================
CLEAN_BUILD=true
BUILD_TYPE="release"

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)      CLEAN_BUILD=true;  shift ;;
        --no-clean)   CLEAN_BUILD=false; shift ;;
        fast)         CLEAN_BUILD=false; shift ;;
        debug)        BUILD_TYPE="debug"; shift ;;
        release)      BUILD_TYPE="release"; shift ;;
        --help|-h)
            echo "用法: $0 [--clean|--no-clean] [release|debug] | fast"
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [--clean|--no-clean] [release|debug] | fast"
            exit 1
            ;;
    esac
done

# ==================== 颜色输出 ====================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC}  $(date '+%H:%M:%S') $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $(date '+%H:%M:%S') $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $(date '+%H:%M:%S') $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $(date '+%H:%M:%S') $*"; }

# ==================== 检测操作系统 ====================
log_info "检测操作系统..."
OS="$(uname -s)"
case "$OS" in
    Linux*)                PLATFORM="linux"  ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM="windows" ;;
    Windows_NT)            PLATFORM="windows" ;;
    *)
        log_error "不支持的操作系统: $OS"
        exit 1
        ;;
esac
log_ok "当前平台: $PLATFORM"

# ==================== 路径解析 ====================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
SRC_DIR="${PROJECT_DIR}/mac_lookup"
APP_NAME="mac_lookup"

log_info "项目根目录: ${PROJECT_DIR}"
log_info "源码目录:   ${SRC_DIR}"

# ==================== 前置检查 ====================
log_info "执行前置检查..."

if [ ! -d "$SRC_DIR" ]; then
    log_error "源码目录不存在: ${SRC_DIR}"
    exit 1
fi
log_ok "源码目录存在"

if [ ! -f "${SRC_DIR}/mac_lookup.pro" ]; then
    log_error "项目文件不存在: mac_lookup.pro"
    exit 1
fi
log_ok "项目文件存在"

if [ ! -d "${SRC_DIR}/data" ]; then
    log_warn "data/ 目录不存在，构建将继续但可能缺少数据文件"
else
    FILE_COUNT=$(find "${SRC_DIR}/data" -type f 2>/dev/null | wc -l)
    log_ok "data/ 目录存在，包含 ${FILE_COUNT} 个文件"
fi

# ==================== 平台相关配置 ====================
if [ "$PLATFORM" = "windows" ]; then
    # 检测 Qt MinGW 路径
    if [ -z "$QT_MINGW_PATH" ]; then
        for candidate in \
            "D:\\software\\Qt\\Qt512\\5.12.12\\mingw73_64" \
            "C:\\Qt\\5.12.12\\mingw73_64" \
            "D:\\Qt\\5.12.12\\mingw73_64" \
            "D:/software/Qt/Qt512/5.12.12/mingw73_64" \
            "C:/Qt/5.12.12/mingw73_64"; do
            if [ -d "$candidate" ]; then
                QT_MINGW_PATH="$candidate"
                break
            fi
        done
    fi

    if [ -z "$QT_MINGW_PATH" ]; then
        log_error "未找到 Qt MinGW，请设置 QT_MINGW_PATH 环境变量"
        exit 1
    fi

    # 转换为 POSIX 路径用于 Git Bash
    QT_MINGW_POSIX="$(cd "$QT_MINGW_PATH" 2>/dev/null && pwd || echo "$QT_MINGW_PATH")"
    QT_BIN="${QT_MINGW_POSIX}/bin"

    # MinGW 路径
    MINGW_BIN=""
    for candidate in \
        "${QT_MINGW_POSIX}/../../Tools/mingw730_64/bin" \
        "${QT_MINGW_POSIX}/../../Tools/mingw*/bin"; do
        for d in $candidate; do
            if [ -d "$d" ]; then MINGW_BIN="$d"; break 2; fi
        done
    done

    export PATH="${QT_BIN}:${MINGW_BIN}:${PATH}"
    QMAKE="${QT_BIN}/qmake"
    MAKE_CMD="${MINGW_BIN}/mingw32-make -j4"
    log_ok "Qt 路径: ${QT_MINGW_PATH}"

    BUILD_DIR="${PROJECT_DIR}/build-mac_lookup"
    SPEC="win32-g++"
    EXE_NAME="${APP_NAME}.exe"
else
    QMAKE="qmake"
    MAKE_CMD="make -j$(nproc 2>/dev/null || echo 4)"
    BUILD_DIR="${PROJECT_DIR}/build-mac_lookup"
    SPEC="linux-g++"
    EXE_NAME="${APP_NAME}"
fi

log_info "输出目录: ${BUILD_DIR}"
log_info "构建类型: ${BUILD_TYPE}"

# ==================== 清理 ====================
if [ "$CLEAN_BUILD" = true ]; then
    log_info "清理旧构建..."
    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"
    log_ok "清理完成"
else
    mkdir -p "${BUILD_DIR}"
    log_info "使用缓存构建（--no-clean）"
fi

cd "${BUILD_DIR}"

# ==================== qmake ====================
log_info "[1/3] 执行 qmake..."
${QMAKE} "${SRC_DIR}/mac_lookup.pro" -spec "${SPEC}" "CONFIG+=${BUILD_TYPE}" "DESTDIR=${BUILD_DIR}"
if [ $? -ne 0 ]; then
    log_error "qmake 失败"
    exit 1
fi
log_ok "qmake 完成"

# ==================== 编译 ====================
log_info "[2/3] 编译中..."
eval "${MAKE_CMD}"
if [ $? -ne 0 ]; then
    log_error "编译失败"
    exit 1
fi
log_ok "编译完成"

# ==================== 查找可执行文件 ====================
EXE_PATH=""
for candidate in \
    "${BUILD_DIR}/${EXE_NAME}" \
    "${BUILD_DIR}/${BUILD_TYPE}/${EXE_NAME}" \
    "${BUILD_DIR}/release/${EXE_NAME}" \
    "${BUILD_DIR}/debug/${EXE_NAME}"; do
    if [ -f "$candidate" ]; then
        EXE_PATH="$candidate"
        break
    fi
done

if [ -z "$EXE_PATH" ]; then
    log_error "未找到可执行文件: ${EXE_NAME}"
    exit 1
fi

OUT_DIR=$(dirname "${EXE_PATH}")
log_ok "可执行文件: ${EXE_PATH}"

# ==================== 复制 data 文件 ====================
log_info "[3/3] 复制资源文件..."
mkdir -p "${OUT_DIR}/data"
if [ -d "${SRC_DIR}/data" ]; then
    cp -f "${SRC_DIR}/data/"* "${OUT_DIR}/data/"
fi
log_ok "资源文件复制完成"

# ==================== 部署动态库 ====================
log_info "部署运行时依赖..."

if [ "$PLATFORM" = "windows" ]; then
    WINDEPLOYQT="${QT_BIN}/windeployqt"
    if [ -x "$WINDEPLOYQT" ] || [ -f "$WINDEPLOYQT" ]; then
        "${WINDEPLOYQT}" "${EXE_PATH}"
        log_ok "windeployqt 部署完成"
    else
        log_warn "windeployqt 未找到，请手动复制 Qt DLL"
    fi
else
    QT_LIB_DIR=$(qmake -query QT_INSTALL_LIBS 2>/dev/null || echo "")
    if [ -n "$QT_LIB_DIR" ] && [ -d "$QT_LIB_DIR" ]; then
        BUNDLED=0
        for lib in Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5Concurrent Qt5Svg; do
            LIB_FILE=$(find "$QT_LIB_DIR" -maxdepth 1 -name "${lib}.so.*" -type f 2>/dev/null | head -1)
            if [ -n "$LIB_FILE" ]; then
                cp -L "$LIB_FILE" "${OUT_DIR}/"
                BUNDLED=$((BUNDLED + 1))
            fi
        done
        log_ok "已捆绑 ${BUNDLED} 个 Qt 共享库"
    fi
    chmod +x "${EXE_PATH}"
fi

FILE_SIZE=$(du -sh "${BUILD_DIR}" | cut -f1)
log_ok "分发包已就绪: ${BUILD_DIR} (${FILE_SIZE})"

# ==================== 结果 ====================
echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN} 构建完成！${NC}"
echo -e "${GREEN}============================================${NC}"
echo -e "  平台:     ${PLATFORM}"
echo -e "  构建类型: ${BUILD_TYPE}"
echo -e "  可执行文件: ${BUILD_DIR}/${EXE_NAME}"
echo ""
echo "  运行:"
echo "    ${BUILD_DIR}/${EXE_NAME}"
echo -e "${GREEN}============================================${NC}"
