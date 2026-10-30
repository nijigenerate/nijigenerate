option(NIJIGENERATE_SDL3_BOUNDARY_PROBE "Build the installed SDL3 window boundary probe" OFF)
if(BUILD_TESTING AND NIJIGENERATE_SDL3_BOUNDARY_PROBE)
    find_package(SDL3 CONFIG REQUIRED)
    add_executable(nijigenerate_sdl3_boundary_probe tests/sdl3_boundary_probe.cpp)
    target_link_libraries(nijigenerate_sdl3_boundary_probe PRIVATE SDL3::SDL3 nijigenerate_native_options)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_sdl3_boundary_probe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:nijigenerate_sdl3_boundary_probe>")
    endif()
    add_test(NAME native_sdl3_boundary COMMAND nijigenerate_sdl3_boundary_probe)
endif()
