cmake_minimum_required(VERSION 3.5.1)
project(cctoml VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

# 添加头文件目录
include_directories(include)

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cc")

# 创建库目标
add_library(cctoml STATIC ${SOURCES})

# 添加头文件目录
target_include_directories(cctoml
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 添加示例
add_subdirectory(examples)
add_subdirectory(test)