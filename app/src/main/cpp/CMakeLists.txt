cmake_minimum_required(VERSION 3.18.1)

project(FHook)

# 解决16k对齐的问题
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展


# --------------- 测试 -----------
add_library(
        ft001          # 库名称（必须与System.loadLibrary参数一致）
        SHARED
        ftest/t01.cpp
)

target_link_libraries(
        ft001
        android
        log
)
