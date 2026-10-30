add_library(nijigenerate_lock_declarations INTERFACE)
target_include_directories(nijigenerate_lock_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_lock_declarations INTERFACE cxx_std_20)
target_sources(nijigenerate_lock_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/lock.hpp")
target_link_libraries(nijigenerate_lock_declarations INTERFACE nijigenerate_native_options)
add_library(nijigenerate_lock STATIC src/nijigenerate/widgets/lock.cpp)
target_link_libraries(nijigenerate_lock PUBLIC nijigenerate_lock_declarations PRIVATE nijigenerate_label)
if(BUILD_TESTING)
    add_library(nijigenerate_lock_header_probe OBJECT tests/lock_header_probe.cpp)
    target_link_libraries(nijigenerate_lock_header_probe PRIVATE nijigenerate_lock_declarations)
    add_executable(nijigenerate_lock_imgui_probe tests/lock_imgui_probe.cpp)
    target_link_libraries(nijigenerate_lock_imgui_probe PRIVATE nijigenerate_lock nijigenerate_imgui)
    add_test(NAME native_lock_imgui_contract COMMAND nijigenerate_lock_imgui_probe)
    add_executable(nijigenerate_lock_recording_probe tests/lock_recording_probe.cpp src/nijigenerate/widgets/lock.cpp)
    target_include_directories(nijigenerate_lock_recording_probe PRIVATE tests/lock_recording src)
    target_link_libraries(nijigenerate_lock_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_lock_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget lock --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_lock_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/lock-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_lock_project DEPENDS nijigenerate_lock
        nijigenerate_lock_header_probe nijigenerate_lock_imgui_probe nijigenerate_lock_recording_probe)
endif()
