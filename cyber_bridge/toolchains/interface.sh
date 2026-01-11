#!/usr/bin/env bash
# set -euxo pipefail # enable for debug
set +e

platform="${1:-x86}" # orinx thor x86 j6m
build_type="${2:-Release}" # Release Debug RelWithDebInfo MinSizeRel
clear_build="${3:-false}" # true or false

common_proto_path=${PROTO_ROOT_DIR}/common

build_proto() {
    local proto_path=$1
    local out_path=$2

    BUILD_PATH=${BUILD_DIR}/${proto_path}
    if [ ! -d $BUILD_PATH ]; then
        mkdir -p $BUILD_PATH
    fi
    echo "BUILD_PATH: $BUILD_PATH"
    echo "INSTALL_DIR: $INSTALL_DIR"
    if [ "$clear_build" == "true" ]; then # 清除install文件及build文件，同时退出
        rm -rf $BUILD_PATH
        echo -e "${COLOR_BLUE}Clear build: ${COLOR_RESET}${COLOR_GREEN}${proto_path}${COLOR_RESET}"
        return 0
    fi
    cd $BUILD_PATH

# 自动生成一个临时 CMakeLists.txt
cat > CMakeLists.txt <<EOF
cmake_minimum_required(VERSION 3.16)
project(${proto_path})
set(PROTO_LIB_NAME \${PROJECT_NAME})
include("\${PROTO_MAKE_FILE}")
EOF

    echo -e "${COLOR_BLUE}Building interface: ${COLOR_RESET}${COLOR_GREEN}${proto_path}${COLOR_RESET}"
    if [ "$ENABLE_CROSS_COMPILER" == "ON" ]; then
        cmake -G Ninja \
            -DPROTO_SOURCE_DIR="${PROTO_ROOT_DIR}/${proto_path}" \
            -DPROTO_INCLUDE_DIR="${common_proto_path}" \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
            -DPLATFORM=${platform} \
            -DCMAKE_C_FLAGS="-Wno-unused-value -Wall -Wno-error ${COLOR_FLAGS}" \
            -DCMAKE_CXX_FLAGS="-Wno-unused-value -Wall -Wno-error ${COLOR_FLAGS}" \
            -DCMAKE_TOOLCHAIN_FILE="${WORK_SPACE}/toolchains/cross-toolchain.cmake" \
            -DPROTO_MAKE_FILE="${WORK_SPACE}/toolchains/depend_proto.cmake" \
            .
    else
        cmake -G Ninja \
            -DPROTO_SOURCE_DIR="${PROTO_ROOT_DIR}/${proto_path}" \
            -DPROTO_INCLUDE_DIR="${common_proto_path}" \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
            -DCMAKE_C_COMPILER="gcc-9" \
            -DCMAKE_CXX_COMPILER="g++-9" \
            -DPLATFORM=${platform} \
            -DCMAKE_C_FLAGS="-Wno-unused-value -Wall -Wno-error ${COLOR_FLAGS}" \
            -DCMAKE_CXX_FLAGS="-Wno-unused-value -Wall -Wno-error ${COLOR_FLAGS}" \
            -DPROTO_MAKE_FILE="${WORK_SPACE}/toolchains/depend_proto.cmake" \
            .
    fi
    # 执行编译
    ninja install
    # 检查 cmake 的返回值
    if [ $? -ne 0 ]; then
        echo -e "${COLOR_RED}CMake configuration error for ${proto_path} $1${COLOR_RESET}"
        exit 1
    fi
}

build_proto "adc_proto" "adc_proto"   # 上层接口
build_proto "adf_proto" "adf_proto"   # 底软接口
