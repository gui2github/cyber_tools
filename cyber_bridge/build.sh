#!/usr/bin/env bash
# set -euxo pipefail # enable for debug

# 使用方法 ./build.sh -a x86_64 -t RelWithDebInfo
# 获取当前脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export WORK_SPACE="$(cd "$SCRIPT_DIR" && pwd)"
build_type=Release  # Debug Release RelWithDebInfo MinSizeRel
platform="x86" # orinx thor x86
USE_CYBER_BRIDGE=ON # 如果使用fastdds需要关闭
interface=false # 是否使用接口编译
export CROSS_TOOLS_PATH=/opt/cross-tools  # 交叉编译工具链路径
export PROTO_ROOT_DIR=${WORK_SPACE}/../../proto # proto 文件路径
ENABLE_CROSS_COMPILER=OFF # 是否使用交叉编译
export LIB_ROOT_DIR=/opt/gwm
arch=`arch`

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -p|--platform)
            platform="$2"
            shift
            shift
            ;;
        -i|--interface)
            interface=true
            shift # past argument
            ;;
        -t|--type)
            build_type="$2"
            shift
            shift
        ;;
        *)
        echo "Unknown option: $key"
        exit 1
        ;;
    esac
done

# 检查平台是否支持
if [ "$platform" != "orinx" ] && [ "$platform" != "thor" ] && [ "$platform" != "x86" ]; then
    echo "Unsupported platform: $platform"
    exit 1
fi

if [[ "$platform" != "x86" && "$arch" != "aarch64" ]]; then
    echo "build with cross-compiler"
    export ENABLE_CROSS_COMPILER=ON
else
    export ENABLE_CROSS_COMPILER=OFF
fi

# 设置 protoc 依赖
if [[ "$ENABLE_CROSS_COMPILER" == "ON" || "$platform" == "x86" ]]; then
    protoc_lib_path=${LIB_ROOT_DIR}/third_party/release/x86_2204/protobuf-3.14.0/lib
else
    protoc_lib_path=${LIB_ROOT_DIR}/third_party/release/orinx_6060/protobuf-3.14.0/lib/lib
fi
export LD_LIBRARY_PATH=$protoc_lib_path:$LD_LIBRARY_PATH


export BUILD_DIR=${WORK_SPACE}/build
export INSTALL_DIR=${WORK_SPACE}/foxglove_bridge
mkdir -p ${BUILD_DIR}
mkdir -p ${INSTALL_DIR}/lib


if [ "$interface" = "true" ]; then
    echo "build interface"
    source ${WORK_SPACE}/toolchains/interface.sh ${platform} ${build_type}
fi

# clang 安装  sudo apt-get install clang-12  or gcc-9
cd ${BUILD_DIR}
cmake .. \
    -G "Ninja" \
    -DBUILD_ROOT_DIR=${BUILD_DIR} \
    -DINSTALL_DIR=${INSTALL_DIR} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
    -DPLATFORM=${platform} \
    -DUSE_CYBER_BRIDGE=$USE_CYBER_BRIDGE \
    -DCMAKE_BUILD_TYPE=$build_type \
    -DCMAKE_PROJECT_INCLUDE_BEFORE=${WORK_SPACE}/toolchains/depend.cmake

ninja install -j$(nproc)
