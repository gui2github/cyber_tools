#!/usr/bin/env bash
# set -euxo pipefail # enable for debug

# 使用方法: ./run.sh -i 192.168.1.1 -p 8765

#设置传入参数
fox_addr=127.0.0.1
fox_port=8765

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -i|--ip)
        fox_addr="$2"
        shift
        shift
        ;;
        -p|--port)
        fox_port="$2"
        shift
        shift
        ;;
        *)
        echo "Unknown option: $key"
        exit 1
        ;;
    esac
done

# 获取当前脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 获取项目根目录（假设 run.sh 在 scripts/ 下）
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
echo "Project root directory: $ROOT_DIR"

# 根据当前系统架构及ubuntu版本选择合适的二进制文件
source /etc/os-release
if [[ "$VERSION_ID" == "22.04" ]]; then
    source /opt/gwm/adcos/release/x86_2204/setup.bash
elif [[ "$VERSION_ID" == "24.04" ]]; then
    if [[ "$(uname -m)" == "aarch64" ]]; then
        source /opt/gwm/adcos/release/thor_u/setup.bash
    else
        source /opt/gwm/adcos/release/x86_2404/setup.bash
    fi
else
    source /opt/gwm/adcos/release/orinx_6060/setup.bash
fi

#LOG 设置
current_time=$(date +"%Y%m%d_%H%M%S")
export HAVP_LOG_LEVEL=INFO    # DEBUG,VERBOSE,INFO,WARNING,ERROR,FATAL
export HAVP_LOG_OUTPUT_TYPE=screen   # screen,file,both
export HAVP_LOG_PATH=/data/log/$current_time

# 添加链接库
if [ -d "$ROOT_DIR/lib" ]; then
    # export LD_LIBRARY_PATH=$ROOT_DIR/lib:$LD_LIBRARY_PATH
    $ROOT_DIR/bin/fox_bridge -i $fox_addr -p $fox_port
else
    # export LD_LIBRARY_PATH=$ROOT_DIR/../../lib:$LD_LIBRARY_PATH
    # 运行
    $ROOT_DIR/../../bin/fox_bridge -i $fox_addr -p $fox_port
fi
