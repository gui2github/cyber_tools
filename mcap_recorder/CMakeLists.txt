cmake_minimum_required(VERSION 3.16)
project(mcap_recorder)
# add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)

# 禁用 MCAP 压缩支持
# add_definitions(-DMCAP_COMPRESSION_NO_LZ4)
# add_definitions(-DMCAP_COMPRESSION_NO_ZSTD)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -fPIC )
endif()

# 包含头文件目录
include_directories( SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3dparty
    ${CMAKE_CURRENT_SOURCE_DIR}/3dparty/mcap/include
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


# 添加可执行文件
set(SOURCES
    src/main.cpp
    src/arg_parser.cpp
    src/cyber_to_mcap_converter.cpp
    src/mcap_to_cyber_converter.cpp
    src/mcap_recorder.cpp
    src/mcap_player.cpp
    src/mcap_impl.cpp
    # 3dparty/backward-cpp/backward.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# 链接第三方库
target_link_libraries(${PROJECT_NAME}
    protobuf
    ${CYBER_COMPOSITE_LIBS}
    pthread
)


install(TARGETS ${PROJECT_NAME}
  DESTINATION
)

# install(DIRECTORY scripts
#   DESTINATION share/${PROJECT_NAME}
# )
