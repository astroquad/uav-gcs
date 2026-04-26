cmake_minimum_required(VERSION 3.16)

project(uav_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

function(ensure_header_dependency dep_name dep_url dep_include_subdir dep_header out_var)
    set(include_var "${dep_name}_INCLUDE_DIR")
    find_path(${include_var} ${dep_header})
    if(${include_var})
        set(${out_var} "${${include_var}}" PARENT_SCOPE)
        return()
    endif()

    FetchContent_Declare(
        ${dep_name}
        URL ${dep_url}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TLS_VERIFY FALSE
    )
    FetchContent_MakeAvailable(${dep_name})
    FetchContent_GetProperties(${dep_name} SOURCE_DIR dep_source_dir)
    set(${out_var} "${dep_source_dir}/${dep_include_subdir}" PARENT_SCOPE)
endfunction()

ensure_header_dependency(
    nlohmann_json
    "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
    "include"
    "nlohmann/json.hpp"
    NLOHMANN_JSON_INCLUDE_DIR
)

ensure_header_dependency(
    tomlplusplus
    "https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz"
    "include"
    "toml++/toml.hpp"
    TOMLPLUSPLUS_INCLUDE_DIR
)

add_library(gcs_dependencies INTERFACE)
target_include_directories(gcs_dependencies INTERFACE
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${TOMLPLUSPLUS_INCLUDE_DIR}
)

add_library(gcs_core STATIC
    src/common/NetworkConfig.cpp
    src/network/UdpTelemetryReceiver.cpp
    src/protocol/TelemetryMessage.cpp
)

target_include_directories(gcs_core PUBLIC
    src
)

target_link_libraries(gcs_core PUBLIC gcs_dependencies)

if(WIN32)
    target_link_libraries(gcs_core PUBLIC ws2_32)
endif()

find_package(OpenCV QUIET COMPONENTS core imgcodecs highgui imgproc)

set(GCS_VIDEO_UI_SOURCE "")
set(GCS_VIDEO_LIBS gcs_dependencies)
if(OpenCV_FOUND)
    set(GCS_VIDEO_UI_SOURCE src/ui/VideoWindow.cpp)
    list(APPEND GCS_VIDEO_LIBS ${OpenCV_LIBS})
elseif(WIN32)
    set(GCS_VIDEO_UI_SOURCE src/ui/VideoWindowWin32.cpp)
    list(APPEND GCS_VIDEO_LIBS user32 gdi32 ole32 windowscodecs)
    message(STATUS "OpenCV was not found; using Win32 WIC video window backend.")
else()
    message(WARNING "OpenCV was not found; uav_gcs_video will not be built.")
endif()

if(GCS_VIDEO_UI_SOURCE)
    add_library(gcs_video STATIC
        src/video/GcsDiscoveryBeacon.cpp
        src/video/JpegFrameReassembler.cpp
        src/video/UdpMjpegReceiver.cpp
        src/video/VideoPacket.cpp
        ${GCS_VIDEO_UI_SOURCE}
        src/app/VideoViewerApp.cpp
    )
    target_include_directories(gcs_video PUBLIC src)
    target_link_libraries(gcs_video PUBLIC ${GCS_VIDEO_LIBS})
    if(WIN32)
        target_link_libraries(gcs_video PUBLIC ws2_32)
    endif()

    add_executable(uav_gcs_video
        src/video_main.cpp
    )
    target_link_libraries(uav_gcs_video PRIVATE gcs_core gcs_video)
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
