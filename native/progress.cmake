add_library(nijigenerate_progress STATIC src/nijigenerate/widgets/progress.cpp)
target_include_directories(nijigenerate_progress PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_progress PUBLIC cxx_std_20)
target_sources(nijigenerate_progress PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/progress.hpp")
target_link_libraries(nijigenerate_progress PUBLIC nijigenerate_imgui nijigenerate_native_options)
if(BUILD_TESTING)
    add_library(nijigenerate_progress_header_probe OBJECT tests/progress_header_probe.cpp)
    target_link_libraries(nijigenerate_progress_header_probe PRIVATE nijigenerate_progress)
    add_executable(nijigenerate_progress_imgui_probe tests/progress_imgui_probe.cpp)
    target_link_libraries(nijigenerate_progress_imgui_probe PRIVATE nijigenerate_progress)
    add_test(NAME native_progress_imgui_contract COMMAND nijigenerate_progress_imgui_probe)
    add_executable(nijigenerate_progress_recording_probe tests/progress_recording_probe.cpp
        src/nijigenerate/widgets/progress.cpp)
    target_include_directories(nijigenerate_progress_recording_probe PRIVATE tests/progress_recording src)
    target_link_libraries(nijigenerate_progress_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_INMATH_SOURCE_DIR)
        add_test(NAME native_progress_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_progress.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_progress_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/progress-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_progress_project DEPENDS nijigenerate_progress
        nijigenerate_progress_header_probe nijigenerate_progress_imgui_probe nijigenerate_progress_recording_probe)
endif()
