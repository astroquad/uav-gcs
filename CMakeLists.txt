cmake_minimum_required(VERSION 3.16)

project(uav_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gcs_core STATIC
    src/common/NetworkConfig.cpp
    src/network/UdpTelemetryReceiver.cpp
    src/protocol/TelemetryMessage.cpp
)

target_include_directories(gcs_core PUBLIC
    src
)

if(WIN32)
    target_link_libraries(gcs_core PUBLIC ws2_32)
endif()

add_executable(uav_gcs
    src/main.cpp
)

target_link_libraries(uav_gcs PRIVATE gcs_core)

option(BUILD_TOOLS "Build GCS development tools" ON)
if(BUILD_TOOLS)
    foreach(tool
        mock_onboard
        log_replayer
    )
        add_executable(${tool} tools/${tool}.cpp)
        target_include_directories(${tool} PRIVATE src)
        if(WIN32)
            target_link_libraries(${tool} PRIVATE ws2_32)
        endif()
    endforeach()
endif()

option(BUILD_TESTS "Build GCS tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
