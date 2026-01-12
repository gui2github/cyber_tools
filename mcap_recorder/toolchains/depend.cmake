if(NOT PLATFORM MATCHES "^(orinx|thor|x86)$")
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}. Supported: orinx, thor, x86")
endif()

if (NOT DEFINED CROSS_TOOLS_PATH)
    set(CROSS_TOOLS_PATH /opt/cross-tools)
endif()

if (PLATFORM MATCHES "^(orinx|thor)$")
    set(TARGET_ARCH aarch64)
    if (PLATFORM MATCHES "^(orinx)$")
        set(FOLDER_NAME orinx_6060)
        set(CUDA_VERSION 11.4)
    else()
        set(FOLDER_NAME thor_u)
        set(CUDA_VERSION 12.8)
    endif()
else()
    set(TARGET_ARCH x86)
    set(FOLDER_NAME x86_2204)
endif()

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "Using ccache: ${CCACHE_PRObianyiGRAM}")
endif()

if (NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64" AND ${TARGET_ARCH} STREQUAL "aarch64")
    message(STATUS "TARGET_ARCH is aarch64, but CMAKE_HOST_SYSTEM_PROCESSOR is ${CMAKE_HOST_SYSTEM_PROCESSOR} use cross compiler!")
    set(ENABLE_CROSS_COMPILER ON)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cross-toolchain.cmake)
    # include(${CMAKE_CURRENT_LIST_DIR}/cross-toolchain.cmake)
else()
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
    set(ENABLE_CROSS_COMPILER OFF)
endif()

# 设置 c/c++ 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


if (NOT DEFINED LIB_ROOT_DIR)
    set(LIB_ROOT_DIR /opt/gwm)
endif()

set(THIRD_PARTY_ROOT ${LIB_ROOT_DIR}/third_party/release/${FOLDER_NAME}/)
set(ADCOS_ROOT ${LIB_ROOT_DIR}/adcos/release/${FOLDER_NAME}/)

if(${TARGET_ARCH} STREQUAL "aarch64" OR ${ENABLE_CROSS_COMPILER})
    set(DRIVEOS_PATH ${CROSS_TOOLS_PATH}/${PLATFORM}/drive-linux)
    set(CUDA_PATH ${CROSS_TOOLS_PATH}/${PLATFORM}/cuda-${CUDA_VERSION})
else()
    set(CUDA_PATH /usr/local/cuda-11.8/lib64)
endif()

set(CYBER_COMPOSITE_LIBS
    cyber
    glog
    gflags
    # util
    fastrtps
    # brpc
    # gwm_log
    zstd
    lz4
    bvar
)

set (FASTDDS_LIBS
    fastcdr
)

set (ADC_PROTOS
    adc_proto
    adf_proto
)
# 设置系统库搜索路径
include_directories( SYSTEM
    ${ADCOS_ROOT}/include
    # adcos depend
    ${THIRD_PARTY_ROOT}/protobuf-3.14.0/include
    ${THIRD_PARTY_ROOT}/Fast-CDR-2.2.2/include
    ${THIRD_PARTY_ROOT}/Fast-DDS-2.14.4/include
    ${THIRD_PARTY_ROOT}/gflags-2.2.2/include
    # ${THIRD_PARTY_ROOT}/glog-0.4.0/include
    ${THIRD_PARTY_ROOT}/brpc-1.12.1/include
    ${THIRD_PARTY_ROOT}/tinyxml2-6.0.0/include
    ${THIRD_PARTY_ROOT}/yaml-0.7.0/include
    ${THIRD_PARTY_ROOT}/zstd-1.4.7/include
    ${THIRD_PARTY_ROOT}/lz4-1.10.0/include
    # ${THIRD_PARTY_ROOT}/openssl-3.1.1/include
    ${THIRD_PARTY_ROOT}/eigen-3.4.0/include
    ${THIRD_PARTY_ROOT}/eigen-3.4.0/include/eigen3
    ${THIRD_PARTY_ROOT}/nlohmann_json-3.12.0/include
    # proto depend
    ${CMAKE_INSTALL_PREFIX}/include
    ${CMAKE_INSTALL_PREFIX}/include/adc_proto
    ${CMAKE_INSTALL_PREFIX}/include/adf_proto
    /home/gui/workspace/cyberrt/CyberRT-10.0.0/output/include
    /home/gui/workspace/cyberrt/CyberRT-10.0.0/install/include
)

# 设置库搜索路径
include_directories(
    # common depend
    ${MODULE_PATH}/../
    ${MODULE_PATH}/common
    # ${MODULE_PATH}/state_machine/parking_fsm/include
    # ${MODULE_PATH}/hmi_server/parking/include
    # ${MODULE_PATH}/hmi_server/parking/BehaviorTree/
)

link_directories(
    ${ADCOS_ROOT}/lib
    # adcos depend
    ${THIRD_PARTY_ROOT}/protobuf-3.14.0/lib
    ${THIRD_PARTY_ROOT}/Fast-CDR-2.2.2/lib
    ${THIRD_PARTY_ROOT}/Fast-DDS-2.14.4/lib
    ${THIRD_PARTY_ROOT}/gflags-2.2.2/lib
    ${THIRD_PARTY_ROOT}/glog-0.4.0/lib
    ${THIRD_PARTY_ROOT}/zstd-1.4.7/lib
    ${THIRD_PARTY_ROOT}/brpc-1.12.1/lib
    ${THIRD_PARTY_ROOT}/tinyxml2-6.0.0/lib
    ${THIRD_PARTY_ROOT}/yaml-0.7.0/lib
    ${THIRD_PARTY_ROOT}/openssl-3.1.1/lib
    ${THIRD_PARTY_ROOT}/leveldb-1.23/lib
    ${THIRD_PARTY_ROOT}/memory-0.7.3/lib
    ${THIRD_PARTY_ROOT}/memory-0.7.3/lib64
    ${THIRD_PARTY_ROOT}/zlib-1.2.13/lib
    ${THIRD_PARTY_ROOT}/lz4-1.10.0/lib
    ${THIRD_PARTY_ROOT}/uuid-2.41/lib
    # modules and proto depend
    ${CMAKE_INSTALL_PREFIX}/lib
    # ${CMAKE_INSTALL_PREFIX}/lib/algo/opt/statemachine/lib
    # ${CMAKE_INSTALL_PREFIX}/lib/algo/opt/common/lib/
    # ${CMAKE_INSTALL_PREFIX}/lib/modules/hmi_server/parking/lib/
    /home/gui/workspace/cyberrt/CyberRT-10.0.0/output/lib
    /home/gui/workspace/cyberrt/CyberRT-10.0.0/install/lib
)

add_link_options(-Wl,-rpath-link,${ADCOS_ROOT}/lib
    -Wl,-rpath-link,${ADCOS_ROOT}/thirdparty_lib
)

if (${TARGET_ARCH} STREQUAL "aarch64" OR ${ENABLE_CROSS_COMPILER})
    include_directories( SYSTEM
        # orinx driveos depend
        ${DRIVEOS_PATH}/include
        ${DRIVEOS_PATH}/include/nvmedia_6x
        # cuda depend
        ${CUDA_PATH}/targets/aarch64-linux/include
    )

    link_directories(
        # orinx driveos depend
        ${DRIVEOS_PATH}/lib-target
        # cuda depend
        ${CUDA_PATH}/targets/aarch64-linux/lib
    )

    add_link_options(-Wl,-rpath-link,${DRIVEOS_PATH}/lib-target
        -Wl,-rpath-link,${CUDA_PATH}/targets/aarch64-linux/lib
    )

    set(DRIVEOS_LIBS
        nvscibuf
        nvscisync
        nvmedia2d
        nvsciipc
        nvscistream
        nvplayfair
        nvsipl
        nvsipl_query
        nvmedia_iep_sci
        nvmedia_ijpe_sci
    )

    set(EGL_LIBRARIES
        EGL
    )

    set(GLESv2_LIBRARIES
        GLESv2
    )

    set(X11_LIBRARIES
        X11
        X11-xcb
        xcb
        Xau
        Xdmcp
    )
else()
    include_directories( SYSTEM
        # cuda depend
        ${CUDA_PATH}/include
    )
    link_directories(
        # cuda depend
        ${CUDA_PATH}/lib64
    )
endif()


set(CUDA_LIBS
    cudart
    cublas
    cudnn
)

set(TRT_LIBS
    nvinfer
    nvonnxparser
    nvparsers
    nvinfer_plugin
)
