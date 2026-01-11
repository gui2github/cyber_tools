cmake_minimum_required(VERSION 3.15)
option(USE_CYBER_BRIDGE "Build with cyber bridge" ON)
project(fox_bridge)
# add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -fPIC )
endif()

# 添加子目录
add_subdirectory(3dparty/foxglove)
message(STATUS "PROTO_ROOT_DIR: ${PROTO_ROOT_DIR}")
# include(${CMAKE_CURRENT_LIST_DIR}/toolchains/depend_proto.cmake)
# include(${CMAKE_CURRENT_LIST_DIR}/toolchains/depend.cmake)

# 包含头文件目录
include_directories( SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/3dparty
    ${CMAKE_CURRENT_SOURCE_DIR}/3dparty/foxglove/include
)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# 添加可执行文件
set(SOURCES
    src/main.cpp
    src/FoxgloveServer.cpp
    # src/MessageConverter.cpp
)
if(USE_CYBER_BRIDGE)
    list(APPEND SOURCES src/CyberBridge.cpp)
else()
    list(APPEND SOURCES src/FastDDSBridge.cpp)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
# add_library(${PROJECT_NAME}  SHARED ${SOURCES})

# 链接第三方库
target_link_libraries(${PROJECT_NAME}
    foxglove_cpp
    ${CYBER_COMPOSITE_LIBS}
    # ${FASTDDS_LIBS}
    # ${ADC_PROTOS}
    protobuf
)

install(TARGETS ${PROJECT_NAME}
  DESTINATION
)

install(DIRECTORY scripts
  DESTINATION share/${PROJECT_NAME}
)


# 添加测试
# option(ENABLE_TEST "Enable compilation test case" OFF)
# if(ENABLE_TEST)
#     add_subdirectory(test)
# endif()
