TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(realpath "$TOP_DIR/../")"

echo "setup for deployed parking software!" 

platform="x86"
THIRD_PARTY_PATH="/opt/gwm/"

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -p|--platform)
            platform="$2"
            shift
            shift
            ;;
        -t|--tp_path)
            THIRD_PARTY_PATH="$2"
            shift
            shift
            ;;
        *)
        echo "Unknown option: $key"
        exit 1
        ;;
    esac
done


# 根据当前系统架构选择合适的二进制文件
if [ "$(uname -m)" = "aarch64" ]; then 
    echo "检测到 ARM64 架构"    
    # 获取 Ubuntu 版本号
    if ! command -v lsb_release &> /dev/null; then
        echo "错误: 无法检测 Ubuntu 版本, lsb_release 命令不可用" >&2
        return 1
    fi
    
    UBUNTU_VERSION=$(lsb_release -r | awk '{print $2}')
    echo "Ubuntu 版本: $UBUNTU_VERSION"

    if [ "$UBUNTU_VERSION" = "24.04" ]; then
        echo "使用 thor_u 配置（Ubuntu 24.04）"
        if [ -f "/data/opt/gwm/adcos/release/thor_u/setup.bash" ]; then
            source "/data/opt/gwm/adcos/release/thor_u/setup.bash"
            THIRD_PARTY_PATH="/data/opt/gwm"
            PLATFORM_DIR=thor_u
            echo "thor_u 环境加载完成"
        else
            echo "错误: thor_u 配置文件不存在 - /opt/gwm/adcos/release/thor_u/setup.bash" >&2
            exit 1
        fi
    else
        echo "使用 orinx_6060 配置(Ubuntu $UBUNTU_VERSION)"
        if [ -f "/opt/gwm/adcos/release/orinx_6060/setup.bash" ]; then
            source "/opt/gwm/adcos/release/orinx_6060/setup.bash"
            THIRD_PARTY_PATH="/data/opt/gwm"
            echo "orinx_6060 环境加载完成"
            PLATFORM_DIR=orinx_6060
        else
            echo "错误: orinx_6060 配置文件不存在 - /opt/gwm/adcos/release/orinx_6060/setup.bash" >&2
            exit 1
        fi
    fi    
else
    source /opt/gwm/adcos/release/x86_2204/setup.bash
    PLATFORM_DIR=x86_2204
fi

export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/adcos/release/${PLATFORM_DIR}/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/gflags-2.2.2/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/Fast-DDS-2.14.4/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/brpc-1.12.1/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/protobuf-3.14.0/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/Fast-CDR-2.2.2/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/tinyxml2-6.0.0/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/memory-0.7.3/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/leveldb-1.23/lib:$LD_LIBRARY_PATH

export PYTHONPATH=$THIRD_PARTY_PATH/third_party/release/${PLATFORM_DIR}/protobuf-3.14.0/lib/python_3d:$PYTHONPATH

