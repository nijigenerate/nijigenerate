# The actual browser implementation and its public include surface.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base AND
        TARGET nijigenerate_inputtext AND TARGET nijigenerate_button AND
        TARGET nijigenerate_drag AND TARGET nijigenerate_toggle)
    option(NG_COMMAND_BROWSER_DIFFERENTIAL "Include the original CommandBrowserDifferential version" OFF)
    if(NG_COMMAND_BROWSER_DIFFERENTIAL)
        target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_COMMAND_BROWSER_DIFFERENTIAL=1)
    endif()
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET command_browser_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_resources.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_internal.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_resource_traits.hpp")
    set(_command_browser_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_internal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_resources.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_inputs.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_outputs.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_execute.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_window.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_reflection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_result_names.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_differential.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_command_browser_sources})
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/command_browser_sort.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_window_base
        nijigenerate_inputtext nijigenerate_button nijigenerate_drag nijigenerate_toggle
        nijigenerate_controller nijigenerate_dummy nijigenerate_tooltip)
    if(MSVC)
        set_property(SOURCE ${_command_browser_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_command_browser_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_command_browser_sources)
endif()
