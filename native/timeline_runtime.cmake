# The body requires both the real GUI and animation domain providers.
add_library(nijigenerate_timeline STATIC src/nijigenerate/widgets/timeline.cpp)
target_link_libraries(nijigenerate_timeline PUBLIC nijigenerate_timeline_declarations
    nijigenerate_imgui nijigenerate_dummy nijigenerate_label nicxlive::nicxlive)
if(MSVC)
    target_compile_options(nijigenerate_timeline PRIVATE /fp:strict)
else()
    target_compile_options(nijigenerate_timeline PRIVATE -ffp-contract=off -fno-fast-math)
endif()
if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
    add_library(nijigenerate_timeline_header_probe OBJECT tests/timeline_header_probe.cpp)
    target_link_libraries(nijigenerate_timeline_header_probe PRIVATE nijigenerate_timeline)
    add_executable(nijigenerate_timeline_runtime_probe tests/timeline_runtime_probe.cpp)
    target_link_libraries(nijigenerate_timeline_runtime_probe PRIVATE nijigenerate_timeline)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_timeline_runtime_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_timeline_runtime_probe>
                $<TARGET_FILE_DIR:nijigenerate_timeline_runtime_probe> COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_timeline_real_imgui_runtime COMMAND nijigenerate_timeline_runtime_probe)
    add_custom_target(nijigenerate_timeline_project DEPENDS nijigenerate_timeline_header_probe
        nijigenerate_timeline_runtime_probe)
endif()
