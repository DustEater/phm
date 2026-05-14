#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# PHM (Platform Health Management) 构建脚本
# 用法: ./build.sh -p <linux|qnx> [options]
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PROJECT_DIR}/build"
OUTPUT_DIR="${PROJECT_DIR}/output"

# 默认值
PLATFORM="linux"
BUILD_TYPE="release"
JOBS=$(nproc 2>/dev/null || echo 4)
CLEAN=0
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF
BUILD_DAEMON=ON
INSTALL=0
VERBOSE=0
TOOLCHAIN_FILE=""

# =============================================================================
# 帮助信息
# =============================================================================
usage() {
    cat <<EOF
 用法: $0 -p <linux|qnx> [选项]

 必要参数:
   -p <platform>    目标平台: linux 或 qnx

 选项:
   -t <type>        构建类型: debug | release (默认: release)
   -j <jobs>        并行编译任务数 (默认: CPU 核数)
   -c               清理构建目录
   -T               启用测试构建
   -E               启用示例构建
   -D               禁用守护进程构建
   -i               安装到 output 目录
   -v               详细输出 (VERBOSE=1)
   -h               显示此帮助信息

 示例:
   ./build.sh -p linux                         # Linux release 构建
   ./build.sh -p linux -t debug -c             # Linux debug 构建, 清理后构建
   ./build.sh -p qnx                           # QNX 交叉编译 (使用 cmake/qnx-aarch64.cmake)
   ./build.sh -p linux -t debug -T -E          # Linux debug, 启用测试和示例
   ./build.sh -p linux -i                      # Linux 构建并安装到 output/
EOF
    exit 0
}

# =============================================================================
# 参数解析
# =============================================================================
while getopts "p:t:j:chTEiDv" opt; do
    case "${opt}" in
        p)
            PLATFORM="${OPTARG}"
            if [[ "${PLATFORM}" != "linux" && "${PLATFORM}" != "qnx" ]]; then
                echo "错误: 平台必须是 'linux' 或 'qnx', 收到: ${PLATFORM}"
                exit 1
            fi
            ;;
        t)
            BUILD_TYPE="${OPTARG}"
            if [[ "${BUILD_TYPE}" != "debug" && "${BUILD_TYPE}" != "release" ]]; then
                echo "错误: 构建类型必须是 'debug' 或 'release', 收到: ${BUILD_TYPE}"
                exit 1
            fi
            ;;
        j)  JOBS="${OPTARG}" ;;
        c)  CLEAN=1 ;;
        T)  BUILD_TESTS=ON ;;
        E)  BUILD_EXAMPLES=ON ;;
        D)  BUILD_DAEMON=OFF ;;
        i)  INSTALL=1 ;;
        v)  VERBOSE=1 ;;
        h)  usage ;;
        *)  usage ;;
    esac
done

shift $((OPTIND - 1))

# =============================================================================
# 转换为 CMake 构建类型
# =============================================================================
case "${BUILD_TYPE}" in
    debug)   CMAKE_BUILD_TYPE="Debug" ;;
    release) CMAKE_BUILD_TYPE="Release" ;;
esac

# =============================================================================
# QNX 平台检查
# =============================================================================
if [[ "${PLATFORM}" == "qnx" ]]; then
    # 默认 QNX SDP 路径（可通过环境变量覆盖）
    QNX_HOST="${QNX_HOST:-/opt/qnx800/host/linux/x86_64}"
    QNX_TARGET="${QNX_TARGET:-/opt/qnx800/target/qnx8}"
    export QNX_HOST
    export QNX_TARGET

    TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/qnx-aarch64.cmake"
    if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
        echo "错误: QNX toolchain 文件不存在: ${TOOLCHAIN_FILE}"
        exit 1
    fi
    echo "[PHM] 配置 QNX 交叉编译..."
    echo "[PHM]   Toolchain: ${TOOLCHAIN_FILE}"
    echo "[PHM]   QNX_HOST:   ${QNX_HOST}"
    echo "[PHM]   QNX_TARGET: ${QNX_TARGET}"
else
    echo "[PHM] 配置 Linux 原生编译..."
fi

# =============================================================================
# 清理
# =============================================================================
if [[ "${CLEAN}" -eq 1 ]]; then
    echo "[PHM] 清理构建目录: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
    echo "[PHM] 清理输出目录: ${OUTPUT_DIR}"
    rm -rf "${OUTPUT_DIR}"
fi

# =============================================================================
# CMake 配置
# =============================================================================
mkdir -p "${BUILD_DIR}"

CMAKE_OPTIONS=(
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DPHM_PLATFORM=${PLATFORM}"
    "-DPHM_BUILD_TESTS=${BUILD_TESTS}"
    "-DPHM_BUILD_EXAMPLES=${BUILD_EXAMPLES}"
    "-DPHM_BUILD_DAEMON=${BUILD_DAEMON}"
)

if [[ -n "${TOOLCHAIN_FILE}" ]]; then
    CMAKE_OPTIONS+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
fi

echo "[PHM]   构建类型: ${BUILD_TYPE}"
echo "[PHM]   构建目录: ${BUILD_DIR}"
echo "[PHM]   输出目录: ${OUTPUT_DIR}"
echo "[PHM]   并行任务: ${JOBS}"
echo "[PHM]   测试:     ${BUILD_TESTS}"
echo "[PHM]   示例:     ${BUILD_EXAMPLES}"
echo "[PHM]   守护进程: ${BUILD_DAEMON}"
echo "[PHM]   安装:     ${INSTALL}"

cd "${BUILD_DIR}"

if [[ "${VERBOSE}" -eq 1 ]]; then
    set -x
fi

cmake "${PROJECT_DIR}" "${CMAKE_OPTIONS[@]}"

# =============================================================================
# 编译
# =============================================================================
echo ""
echo "[PHM] 开始编译 (${JOBS} 并行任务)..."
make -j"${JOBS}"

# =============================================================================
# 安装
# =============================================================================
if [[ "${INSTALL}" -eq 1 ]]; then
    echo ""
    echo "[PHM] 安装到: ${OUTPUT_DIR}"
    rm -rf "${OUTPUT_DIR}"

    # 库文件
    mkdir -p "${OUTPUT_DIR}/lib"
    if [[ -f "${BUILD_DIR}/src/libphm.so" ]]; then
        cp -a "${BUILD_DIR}/src/libphm.so"* "${OUTPUT_DIR}/lib/" 2>/dev/null || true
    fi
    if [[ -f "${BUILD_DIR}/src/libphm.a" ]]; then
        cp "${BUILD_DIR}/src/libphm.a" "${OUTPUT_DIR}/lib/"
    fi

    # 头文件
    mkdir -p "${OUTPUT_DIR}/include/faw/phm"
    cp "${PROJECT_DIR}/include/faw/phm/"*.h "${OUTPUT_DIR}/include/faw/phm/"

    # 守护进程
    if [[ -f "${BUILD_DIR}/daemon/phmd" ]]; then
        mkdir -p "${OUTPUT_DIR}/bin"
        cp "${BUILD_DIR}/daemon/phmd" "${OUTPUT_DIR}/bin/"
    fi

    # 配置文件
    mkdir -p "${OUTPUT_DIR}/etc/phm"
    cp "${PROJECT_DIR}/config/phm_se_config.json" "${OUTPUT_DIR}/etc/phm/"

    # 测试程序
    if [[ "${BUILD_TESTS}" == "ON" ]]; then
        mkdir -p "${OUTPUT_DIR}/tests"
        for test_bin in "${BUILD_DIR}"/tests/phm_test_*; do
            if [[ -x "${test_bin}" ]]; then
                cp "${test_bin}" "${OUTPUT_DIR}/tests/"
            fi
        done
    fi

    # 示例程序
    if [[ "${BUILD_EXAMPLES}" == "ON" ]]; then
        mkdir -p "${OUTPUT_DIR}/examples"
        for exe in "${BUILD_DIR}"/examples/phm_example_*; do
            if [[ -x "${exe}" ]]; then
                cp "${exe}" "${OUTPUT_DIR}/examples/"
            fi
        done
    fi

    echo "[PHM] 安装完成"
fi

# =============================================================================
# 构建摘要
# =============================================================================
echo ""
echo "[PHM] ───────────────────────────────────────────"
echo "[PHM]  构建完成!"
echo "[PHM]  平台:     ${PLATFORM}"
echo "[PHM]  类型:     ${BUILD_TYPE}"
echo "[PHM]  目录:     ${BUILD_DIR}"
echo "[PHM]  产物:"

if [[ -f "${BUILD_DIR}/src/libphm.so" ]]; then
    echo "[PHM]    libphm.so (共享库)"
fi
if [[ -f "${BUILD_DIR}/src/libphm.a" ]]; then
    echo "[PHM]    libphm.a  (静态库)"
fi
if [[ -f "${BUILD_DIR}/daemon/phmd" ]]; then
    echo "[PHM]    phmd     (守护进程)"
fi
if [[ -f "${BUILD_DIR}/examples/phm_example_basic" ]]; then
    echo "[PHM]    phm_example_basic (示例)"
fi

TEST_COUNT=0
for test_bin in "${BUILD_DIR}"/tests/phm_test_*; do
    if [[ -x "${test_bin}" ]]; then
        TEST_COUNT=$((TEST_COUNT + 1))
    fi
done
if [[ "${TEST_COUNT}" -gt 0 ]]; then
    echo "[PHM]    ${TEST_COUNT} 个测试程序 (tests/ 目录)"
fi

if [[ "${INSTALL}" -eq 1 ]]; then
    echo "[PHM]  安装目录: ${OUTPUT_DIR}"
fi

echo "[PHM] ───────────────────────────────────────────"

if [[ "${PLATFORM}" == "linux" && "${BUILD_TESTS}" == "ON" ]]; then
    echo "[PHM] 运行: cd ${BUILD_DIR} && ctest --output-on-failure"
fi
echo ""