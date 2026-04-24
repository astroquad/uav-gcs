cmake_minimum_required(VERSION 3.16)

project(uav_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(uav_gcs
    src/main.cpp
)

target_include_directories(uav_gcs PRIVATE
    src
)

option(BUILD_TOOLS "Build GCS development tools" ON)
if(BUILD_TOOLS)
    foreach(tool
        mock_onboard
        log_replayer
    )
        add_executable(${tool} tools/${tool}.cpp)
        target_include_directories(${tool} PRIVATE src)
    endforeach()
endif()

option(BUILD_TESTS "Build GCS tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
