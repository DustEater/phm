cmake_minimum_required(VERSION 3.16)
project(phm VERSION 1.0.0 LANGUAGES CXX)

# =============================================================================
# 构建选项
# =============================================================================
option(PHM_BUILD_DAEMON   "Build phmd daemon"               ON)
option(PHM_BUILD_TESTS    "Build unit tests"                 ON)
option(PHM_BUILD_EXAMPLES "Build examples"                   OFF)
option(PHM_ENABLE_ASAN    "Enable AddressSanitizer"          OFF)
option(PHM_ENABLE_UBSAN   "Enable UndefinedBehaviorSanitizer" OFF)
option(PHM_ENABLE_TSAN    "Enable ThreadSanitizer"           OFF)
option(PHM_IPC_USE_UNIX   "Use Unix Domain Socket for IPC"   ON)

# 平台检测
if(NOT PHM_PLATFORM)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PHM_PLATFORM "linux")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "QNX")
        set(PHM_PLATFORM "qnx")
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

message(STATUS "PHM Platform: ${PHM_PLATFORM}")
message(STATUS "PHM Version:  ${PROJECT_VERSION}")

# =============================================================================
# C++ 标准
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# 编译器选项
# =============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-unused-parameter)
    add_compile_options(-fPIC)
endif()

# Sanitizer
foreach(SANITIZER ASAN UBSAN TSAN)
    if(${PHM_ENABLE_${SANITIZER}})
        add_compile_options(-fsanitize=${${SANITIZER}})
        add_link_options(-fsanitize=${${SANITIZER}})
    endif()
endforeach()

# =============================================================================
# 包含目录
# =============================================================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# =============================================================================
# 子目录
# =============================================================================
add_subdirectory(src)

if(PHM_BUILD_DAEMON)
    add_subdirectory(daemon)
endif()

if(PHM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PHM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()