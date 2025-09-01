cmake_minimum_required(VERSION 3.18.1)

project(FHook)

# 解决16k对齐的问题
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展


# 定义公共源文件列表
set(COMMON_SOURCES
        FGlobal_flib.cpp
        tools/fsys.cpp

)

add_library(
        fhook          # 库名称（必须与System.loadLibrary参数一致）
        SHARED
        main/fhook.cpp

        ${COMMON_SOURCES}
)

target_link_libraries(
        fhook
        android
        log
)

add_library(
        fhook_agent
        SHARED
        agent/fhook_agent.cpp

        ${COMMON_SOURCES}
)

target_link_libraries(
        fhook_agent
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
