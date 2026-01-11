cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED LIB_ROOT_DIR)
    set(LIB_ROOT_DIR /opt/gwm)
endif()

if (PLATFORM MATCHES "^(orinx|thor)$")
    set(TARGET_ARCH aarch64)
    if (PLATFORM MATCHES "^(orinx)$")
        set(FOLDER_NAME orinx_6060)
    else()
        set(FOLDER_NAME thor_u)
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
else()
    set(ENABLE_CROSS_COMPILER OFF)
endif()

# 设置 c/c++ 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# 设置编译器警告
add_compile_options(
    -Wno-deprecated-declarations
)

# protobuf library config
set(PROTOBUF_ROOT_DIR ${LIB_ROOT_DIR}/third_party/release/${FOLDER_NAME}/protobuf-3.14.0)
include_directories(${PROTOBUF_ROOT_DIR}/include)
link_directories(${PROTOBUF_ROOT_DIR}/lib)
if(${ENABLE_CROSS_COMPILER})
  set(PROTOBUF_COMPILER ${LIB_ROOT_DIR}/third_party/release/x86_2204/protobuf-3.14.0/bin/protoc)
else()
  set(PROTOBUF_COMPILER ${PROTOBUF_ROOT_DIR}/bin/protoc)
endif()

function(protobuf_generate)
  include(CMakeParseArguments)

  set(_options APPEND_PATH DESCRIPTORS)
  set(_singleargs LANGUAGE OUT_VAR EXPORT_MACRO PROTOC_OUT_DIR PLUGIN DESC_OUT_VAR DESC_OUT_DIR)
  if(COMMAND target_sources)
    list(APPEND _singleargs TARGET)
  endif()
  set(_multiargs PROTOS IMPORT_DIRS GENERATE_EXTENSIONS)

  cmake_parse_arguments(protobuf_generate "${_options}" "${_singleargs}" "${_multiargs}" "${ARGN}")

  if(NOT protobuf_generate_PROTOS AND NOT protobuf_generate_TARGET)
    message(SEND_ERROR "Error: protobuf_generate called without any targets or source files")
    return()
  endif()

  if(NOT protobuf_generate_OUT_VAR AND NOT protobuf_generate_TARGET)
    message(SEND_ERROR "Error: protobuf_generate called without a target or output variable")
    return()
  endif()

  if(NOT protobuf_generate_LANGUAGE)
    set(protobuf_generate_LANGUAGE cpp)
  endif()
  string(TOLOWER ${protobuf_generate_LANGUAGE} protobuf_generate_LANGUAGE)

  if(NOT protobuf_generate_PROTOC_OUT_DIR)
    set(protobuf_generate_PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  if(protobuf_generate_EXPORT_MACRO AND protobuf_generate_LANGUAGE STREQUAL cpp)
    set(_dll_export_decl "dllexport_decl=${protobuf_generate_EXPORT_MACRO}:")
  endif()

  if(protobuf_generate_PLUGIN)
    set(_plugin "--plugin=${protobuf_generate_PLUGIN}")
  endif()

  if(NOT protobuf_generate_GENERATE_EXTENSIONS)
    if(protobuf_generate_LANGUAGE STREQUAL cpp)
      set(protobuf_generate_GENERATE_EXTENSIONS .pb.h .pb.cc)
    elseif(protobuf_generate_LANGUAGE STREQUAL python)
      set(protobuf_generate_GENERATE_EXTENSIONS _pb2.py)
    else()
      message(SEND_ERROR "Error: protobuf_generate given unknown Language ${LANGUAGE}, please provide a value for GENERATE_EXTENSIONS")
      return()
    endif()
  endif()

  if(protobuf_generate_TARGET)
    get_target_property(_source_list ${protobuf_generate_TARGET} SOURCES)
    foreach(_file ${_source_list})
      if(_file MATCHES "proto$")
        list(APPEND protobuf_generate_PROTOS ${_file})
      endif()
    endforeach()
  endif()

  if(NOT protobuf_generate_PROTOS)
    message(SEND_ERROR "Error: protobuf_generate could not find any .proto files")
    return()
  endif()

  if(protobuf_generate_APPEND_PATH)
    foreach(_file ${protobuf_generate_PROTOS})
      get_filename_component(_abs_file ${_file} ABSOLUTE)
      get_filename_component(_abs_path ${_abs_file} PATH)
      list(FIND _protobuf_include_path ${_abs_path} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _protobuf_include_path -I ${_abs_path})
      endif()
    endforeach()
  endif()

  foreach(DIR ${protobuf_generate_IMPORT_DIRS})
    get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
    list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
    if(${_contains_already} EQUAL -1)
      list(APPEND _protobuf_include_path -I ${ABS_PATH})
    endif()
  endforeach()

  if(NOT _protobuf_include_path)
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  # 如果启用了 DESCRIPTORS，确保 DESC_OUT_DIR 有默认值
  if(protobuf_generate_DESCRIPTORS AND NOT protobuf_generate_DESC_OUT_DIR)
    set(protobuf_generate_DESC_OUT_DIR ${protobuf_generate_PROTOC_OUT_DIR})
  endif()

  # 创建描述符输出目录
  if(protobuf_generate_DESCRIPTORS AND protobuf_generate_DESC_OUT_DIR)
    file(MAKE_DIRECTORY ${protobuf_generate_DESC_OUT_DIR})
  endif()

  set(_generated_srcs_all)
  set(_generated_desc_files)
  foreach(_proto ${protobuf_generate_PROTOS})
    get_filename_component(_abs_file ${_proto} ABSOLUTE)
    get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
    get_filename_component(_file_full_name ${_proto} NAME)
    string(FIND "${_file_full_name}" "." _file_last_ext_pos REVERSE)
    string(SUBSTRING "${_file_full_name}" 0 ${_file_last_ext_pos} _basename)

    set(_suitable_include_found FALSE)
    foreach(DIR ${_protobuf_include_path})
      if(NOT DIR STREQUAL "-I")
        file(RELATIVE_PATH _rel_dir ${DIR} ${_abs_dir})
        string(FIND "${_rel_dir}" "../" _is_in_parent_folder)
        if(NOT ${_is_in_parent_folder} EQUAL 0)
          set(_suitable_include_found TRUE)
          break()
        endif()
      endif()
    endforeach()

    if(NOT _suitable_include_found)
      message(SEND_ERROR "Error: protobuf_generate could not find any correct proto include directory.")
      return()
    endif()

    set(_generated_srcs)
    foreach(_ext ${protobuf_generate_GENERATE_EXTENSIONS})
      list(APPEND _generated_srcs "${protobuf_generate_PROTOC_OUT_DIR}/${_rel_dir}/${_basename}${_ext}")
    endforeach()
    list(APPEND _generated_srcs_all ${_generated_srcs})

    # 处理描述符文件生成
    set(_desc_file)
    if(protobuf_generate_DESCRIPTORS)
      set(_desc_file "${protobuf_generate_DESC_OUT_DIR}/${_rel_dir}/${_basename}.desc")
      list(APPEND _generated_desc_files ${_desc_file})
    endif()

    # 构建 protoc 命令
    set(_protoc_args)
    list(APPEND _protoc_args --${protobuf_generate_LANGUAGE}_out=${_dll_export_decl}${protobuf_generate_PROTOC_OUT_DIR})
    if(protobuf_generate_DESCRIPTORS)
      list(APPEND _protoc_args --descriptor_set_out=${_desc_file})
    endif()
    if(_plugin)
      list(APPEND _protoc_args ${_plugin})
    endif()
    list(APPEND _protoc_args ${_protobuf_include_path} ${_abs_file})

    add_custom_command(
      OUTPUT ${_generated_srcs} ${_desc_file}
      COMMAND ${PROTOBUF_COMPILER}
      ARGS ${_protoc_args}
      DEPENDS ${_abs_file}
      COMMENT "Running ${protobuf_generate_LANGUAGE} protocol buffer compiler on ${_proto}"
      VERBATIM
    )
  endforeach()

  set_source_files_properties(${_generated_srcs_all} ${_generated_desc_files} PROPERTIES GENERATED TRUE)
  if(protobuf_generate_OUT_VAR)
    set(${protobuf_generate_OUT_VAR} ${_generated_srcs_all} PARENT_SCOPE)
  endif()
  if(protobuf_generate_DESC_OUT_VAR AND protobuf_generate_DESCRIPTORS)
    set(${protobuf_generate_DESC_OUT_VAR} ${_generated_desc_files} PARENT_SCOPE)
  endif()
  if(protobuf_generate_TARGET)
    target_sources(${protobuf_generate_TARGET} PRIVATE ${_generated_srcs_all})
  endif()
endfunction()


# add proto files
if (NOT DEFINED PROTO_SOURCE_DIR)
    set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../proto)
    message( STATUS "PROTO_SOURCE_DIR not specified, using default: ${PROTO_SOURCE_DIR}")
endif()
message(STATUS "PROTO_SOURCE_DIR: ${PROTO_SOURCE_DIR}")
if (NOT DEFINED PROTO_INCLUDE_DIR)
    set(PROTO_INCLUDE_DIR ${PROTO_SOURCE_DIR}/common)
    message( STATUS "PROTO_INCLUDE_DIR not specified, using default: ${PROTO_INCLUDE_DIR}")
endif()
# Create a unique name for the proto library to avoid conflicts with main project
if (NOT DEFINED PROTO_LIB_NAME)
    set(PROTO_LIB_NAME ADC_proto)
endif()
# 先创建空库目标
add_library(${PROTO_LIB_NAME} SHARED "")

# 设置proto文件路径
# file(GLOB ADC_PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/foxglove/*.proto")
file(GLOB_RECURSE COMMON_PROTO_FILES "${PROTO_INCLUDE_DIR}/*.proto")
file(GLOB_RECURSE ADC_PROTO_FILES "${PROTO_SOURCE_DIR}/*.proto")
set(PROTO_SRCS ${COMMON_PROTO_FILES} ${ADC_PROTO_FILES})

# 使用正则匹配排除不需要的文件
foreach (file ${PROTO_SRCS})
    if (file MATCHES "adcos/hmi_can")
        list(REMOVE_ITEM PROTO_SRCS ${file})
    endif()
endforeach()
# message(STATUS "PROTO_SRCS: ${PROTO_SRCS}")

# 设置生成目录
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/schemas)
file(MAKE_DIRECTORY "${GENERATED_DIR}")
# 生成proto文件并添加到目标
protobuf_generate(
    LANGUAGE cpp                                    # 生成语言 （cpp、python、java等）
    TARGET ${PROTO_LIB_NAME}                        # 指定一个已有 CMake target，将 .pb 源文件添加到该 target
    # APPEND_PATH                                     # 是否为每个 .proto 自动加其所在目录作为 -I
    # OUT_VAR PROTO_GEN_FILES                         # 生成文件的变量名
    PROTOS ${PROTO_SRCS}                            # 明确列出的 .proto 文件路径列表
    IMPORT_DIRS ${PROTO_SOURCE_DIR} ${PROTO_SOURCE_DIR}/..                # Include both base and foxglove directories
    PROTOC_OUT_DIR ${GENERATED_DIR}                 # 指定生成的文件存放目录（默认 ${CMAKE_CURRENT_BINARY_DIR}）
)
# 生成 python 代码
# file(MAKE_DIRECTORY "${GENERATED_DIR}_py")
# file(MAKE_DIRECTORY "${GENERATED_DIR}_desc")
# protobuf_generate(
#     LANGUAGE python
#     TARGET ${PROTO_LIB_NAME}
#     PROTOS ${PROTO_SRCS}
#     IMPORT_DIRS ${PROTO_SOURCE_DIR} ${PROTO_SOURCE_DIR}/..
#     PROTOC_OUT_DIR ${GENERATED_DIR}_py
#     DESCRIPTORS  # 启用描述符生成
#     DESC_OUT_DIR ${GENERATED_DIR}_desc
# )

# Add the generated headers directory to include path
target_include_directories(${PROTO_LIB_NAME} PUBLIC
    ${GENERATED_DIR}
)

install(DIRECTORY ${GENERATED_DIR}/
        DESTINATION include/${PROTO_LIB_NAME}
        FILES_MATCHING PATTERN "*.pb.h")

install(TARGETS ${PROTO_LIB_NAME}
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
)
