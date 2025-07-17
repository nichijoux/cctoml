# 添加性能测试
add_executable(toml-test toml-test.cc)
target_link_libraries(toml-test PRIVATE cctoml)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/toml-test-linux-amd64
        DESTINATION ${CMAKE_BINARY_DIR}/test/
        USE_SOURCE_PERMISSIONS)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test.sh
        DESTINATION ${CMAKE_BINARY_DIR}/test/
        USE_SOURCE_PERMISSIONS)