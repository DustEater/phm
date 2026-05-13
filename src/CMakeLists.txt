# libphm.so 构建
# =============================================================================

# 收集平台无关源文件
set(PHM_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/phm_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/state_machine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/data_reporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/health_channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitor_framework.cpp
)

set(PHM_MONITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/monitors/process_lifecycle_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitors/resource_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitors/deadlock_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitors/alive_monitor.cpp
)

set(PHM_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/pal.cpp
)

# 平台特定源文件
if(PHM_PLATFORM STREQUAL "linux")
    list(APPEND PHM_PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/process_linux.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/resource_linux.cpp
    )
    add_definitions(-DPHM_PLATFORM_LINUX=1)
elseif(PHM_PLATFORM STREQUAL "qnx")
    list(APPEND PHM_PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/qnx/process_qnx.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/qnx/resource_qnx.cpp
    )
    add_definitions(-DPHM_PLATFORM_QNX=1)
endif()

# 创建共享库
add_library(phm SHARED
    ${PHM_CORE_SOURCES}
    ${PHM_MONITOR_SOURCES}
    ${PHM_PLATFORM_SOURCES}
)

# 创建静态库（可选）
add_library(phm_static STATIC
    ${PHM_CORE_SOURCES}
    ${PHM_MONITOR_SOURCES}
    ${PHM_PLATFORM_SOURCES}
)

set_target_properties(phm PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   1
    OUTPUT_NAME phm
)

set_target_properties(phm_static PROPERTIES
    OUTPUT_NAME phm
)

target_link_libraries(phm PRIVATE pthread rt)
target_link_libraries(phm_static PRIVATE pthread rt)

# 安装
install(TARGETS phm phm_static
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/faw
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)