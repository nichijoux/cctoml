# 创建示例程序
add_executable(basic_usage basic_usage.cc)
target_link_libraries(basic_usage PRIVATE cctoml)

add_executable(toml_file_io toml_file_io.cc)
target_link_libraries(toml_file_io PRIVATE cctoml)

# 复制示例文件到构建目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.toml
        ${CMAKE_BINARY_DIR}/examples/config.toml COPYONLY)