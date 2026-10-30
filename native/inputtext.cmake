set(inputtext_array_default "${PROJECT_SOURCE_DIR}/../nicxlive/core")
if(NIJIGENERATE_NICXLIVE_SOURCE_DIR)
    set(inputtext_array_default "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core")
endif()
set(NIJIGENERATE_INPUTTEXT_ARRAY_INCLUDE_DIR "${inputtext_array_default}" CACHE PATH
    "Current independent SharedArray include directory for inputtext callback descriptors")
add_library(nijigenerate_inputtext_declarations INTERFACE)
target_compile_features(nijigenerate_inputtext_declarations INTERFACE cxx_std_20)
target_include_directories(nijigenerate_inputtext_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${NIJIGENERATE_INPUTTEXT_ARRAY_INCLUDE_DIR}")
target_link_libraries(nijigenerate_inputtext_declarations INTERFACE nijigenerate_imgui nijigenerate_native_options)
target_sources(nijigenerate_inputtext_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/inputtext.hpp")
add_library(nijigenerate_inputtext_callbacks STATIC src/nijigenerate/widgets/inputtext_callbacks.cpp)
target_link_libraries(nijigenerate_inputtext_callbacks PUBLIC nijigenerate_inputtext_declarations)
target_sources(nijigenerate_inputtext_callbacks PRIVATE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/inputtext_internal.hpp")
add_library(nijigenerate_inputtext_sdl3_window STATIC src/nijigenerate/platform/imgui_sdl3_window.cpp)
target_link_libraries(nijigenerate_inputtext_sdl3_window PUBLIC nijigenerate_inputtext_declarations SDL3::SDL3
    PRIVATE nijigenerate_imgui_sdl3)
target_sources(nijigenerate_inputtext_sdl3_window PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/platform/imgui_sdl3_window.hpp")
if(BUILD_TESTING)
    add_library(nijigenerate_inputtext_header_probe OBJECT tests/inputtext_header_probe.cpp)
    target_link_libraries(nijigenerate_inputtext_header_probe PRIVATE nijigenerate_inputtext_declarations)
    add_executable(nijigenerate_inputtext_callbacks_probe tests/inputtext_callbacks_probe.cpp)
    target_link_libraries(nijigenerate_inputtext_callbacks_probe PRIVATE nijigenerate_inputtext_callbacks)
    add_test(NAME native_inputtext_callbacks_contract COMMAND nijigenerate_inputtext_callbacks_probe)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_inputtext_callbacks_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_inputtext_callbacks.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_inputtext_callbacks_probe>"
                --array-include "${NIJIGENERATE_INPUTTEXT_ARRAY_INCLUDE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/inputtext-callbacks-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_inputtext_project DEPENDS nijigenerate_inputtext_callbacks
        nijigenerate_inputtext_header_probe nijigenerate_inputtext_callbacks_probe)
endif()

add_library(nijigenerate_inputtext STATIC src/nijigenerate/widgets/inputtext.cpp)
target_link_libraries(nijigenerate_inputtext PUBLIC nijigenerate_inputtext_callbacks nijigenerate_dummy
    PRIVATE nijigenerate_inputtext_sdl3_window)
target_sources(nijigenerate_inputtext PRIVATE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/inputtext_buffers.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/inputtext_internal.hpp")
