cmake_minimum_required(VERSION 3.18.1)

project(FHook)

# 解决16k对齐的问题
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展


add_library(
        dexter
        STATIC #  静态库 只生成 .a，不会打包进 APK 后续谁链接它

        dexter/bytecode_encoder.cc
        dexter/code_ir.cc
        dexter/common.cc
        dexter/control_flow_graph.cc
        dexter/debuginfo_encoder.cc
        dexter/dex_bytecode.cc
        dexter/dex_format.cc
        dexter/dex_ir.cc
        dexter/dex_ir_builder.cc
        dexter/dex_utf8.cc
        dexter/instrumentation.cc
        dexter/reader.cc
        dexter/tryblocks_encoder.cc
        dexter/writer.cc

)


# 定义公共源文件列表
set(COMMON_SOURCES
        FGlobal_flib.cpp
        tools/fsys.cpp
)

add_library(
        fhook          # 库名称（必须与System.loadLibrary参数一致）
        SHARED

        ${COMMON_SOURCES}

        main/fhook.cpp
)

target_link_libraries(
        fhook
        android
        log
)

# ------------------------------ fhook_agent
add_library(
        fhook_agent
        SHARED

        ${COMMON_SOURCES}

        agent/fhook_agent.cpp
        agent/finject.cpp
        agent/fir_funs_def.cpp
        agent/fir_funs_do.cpp
        agent/fir_funs_do_impl.cpp
        agent/fir_tools.cpp
        agent/fsmali_printer.cpp
        agent/transform.cpp
        agent/freg_manager.cpp
        agent/freg_allocator.cpp

)

target_link_libraries(
        fhook_agent

        dexter  # 静态库
        z

        android
        log
)


## --------------- 测试 -----------
#add_library(
#        ft002          # 库名称（必须与System.loadLibrary参数一致）
#        SHARED
#        ftest/t01.cpp
#)
#
#target_link_libraries(
#        ft002
#        android
#        log
#)
