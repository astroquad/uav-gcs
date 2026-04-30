add_executable(test_telemetry_line_parse
    test_telemetry_line_parse.cpp
)
target_include_directories(test_telemetry_line_parse PRIVATE ../src)
target_link_libraries(test_telemetry_line_parse PRIVATE gcs_core)
add_test(NAME telemetry_line_parse COMMAND test_telemetry_line_parse)

add_executable(test_video_reassembler
    test_video_reassembler.cpp
    ../src/video/JpegFrameReassembler.cpp
)
target_include_directories(test_video_reassembler PRIVATE ../src)
add_test(NAME video_reassembler COMMAND test_video_reassembler)

if(TARGET gcs_video)
    add_executable(test_line_overlay
        test_line_overlay.cpp
    )
    target_include_directories(test_line_overlay PRIVATE ../src)
    target_link_libraries(test_line_overlay PRIVATE gcs_video gcs_core)
    add_test(NAME line_overlay COMMAND test_line_overlay)

    add_executable(test_intersection_overlay
        test_intersection_overlay.cpp
    )
    target_include_directories(test_intersection_overlay PRIVATE ../src)
    target_link_libraries(test_intersection_overlay PRIVATE gcs_video gcs_core)
    add_test(NAME intersection_overlay COMMAND test_intersection_overlay)
endif()
