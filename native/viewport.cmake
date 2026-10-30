add_library(nijigenerate_viewport STATIC src/nijigenerate/widgets/viewport.cpp)
target_include_directories(nijigenerate_viewport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_viewport PUBLIC cxx_std_20)
target_sources(nijigenerate_viewport PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/viewport.hpp")
target_link_libraries(nijigenerate_viewport PUBLIC nijigenerate_imgui nijigenerate_native_options)
if(BUILD_TESTING)
    add_library(nijigenerate_viewport_header_probe OBJECT tests/viewport_header_probe.cpp)
    target_link_libraries(nijigenerate_viewport_header_probe PRIVATE nijigenerate_viewport)
    add_executable(nijigenerate_viewport_runtime_probe tests/viewport_runtime_probe.cpp)
    target_link_libraries(nijigenerate_viewport_runtime_probe PRIVATE nijigenerate_viewport)
    add_test(NAME native_viewport_real_imgui_runtime COMMAND nijigenerate_viewport_runtime_probe)
    add_custom_target(nijigenerate_viewport_project DEPENDS nijigenerate_viewport_header_probe
        nijigenerate_viewport_runtime_probe)
endif()
