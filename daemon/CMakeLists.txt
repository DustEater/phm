# cmake build for phmd daemon
add_executable(phmd
    main.cpp
    daemon.cpp
    ipc_server.cpp
    alert_aggregator.cpp
)

target_link_libraries(phmd PRIVATE phm pthread rt)

# 安装
install(TARGETS phmd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/config/phm_se_config.json
    DESTINATION etc/phm)