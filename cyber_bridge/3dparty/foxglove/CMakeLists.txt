project(foxglove_cpp LANGUAGES CXX)

set(foxglove_sdk_version 0.16.2)

if (${PLATFORM} MATCHES "x86") 
  set(LIB_DIR amd64)
else()
  set(LIB_DIR aarch64)
endif()

file(GLOB_RECURSE FILE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 2. 只编译对象文件（不链接）
add_library(foxglove_obj OBJECT ${FILE_SOURCES})
target_include_directories(foxglove_obj PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 3. 用对象文件 + 静态库 生成最终动态库
add_library(${PROJECT_NAME} STATIC
    $<TARGET_OBJECTS:foxglove_obj>)   # 把对象文件灌进来

# 4. 链接静态库（关键）
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/${LIB_DIR}/libfoxglove.a)
