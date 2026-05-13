# Tests CMakeLists.txt - 使用系统安装的 Google Test

# 查找系统安装的 Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 辅助函数：添加测试
function(add_phm_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE
        phm_static
        GTest::GTest
        GTest::Main
        pthread
    )
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# 状态机测试
add_phm_test(phm_test_state_machine
    test_state_machine.cpp
)

# 配置解析器测试
add_phm_test(phm_test_config_parser
    test_config_parser.cpp
)

# Monitor 框架测试
add_phm_test(phm_test_monitor_framework
    test_monitor_framework.cpp
)

# 抖动抑制测试
add_phm_test(phm_test_debounce
    test_debounce.cpp
)

# IPC 服务器测试
add_phm_test(phm_test_ipc
    test_ipc_server.cpp
)

# PAL 平台测试（仅在 Linux 上）
if(PHM_PLATFORM STREQUAL "linux")
    add_phm_test(phm_test_pal_linux
        test_pal_linux.cpp
    )
endif()