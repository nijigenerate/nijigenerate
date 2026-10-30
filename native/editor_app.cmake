# Product-source publication only. The owner of the real editor executable calls
# this after supplying its actual module/provider targets; no test target or
# substitute executable is created here.
function(ng_attach_editor_app target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "ng_attach_editor_app requires an existing editor executable target")
    endif()
    get_target_property(_ng_editor_type "${target}" TYPE)
    if(NOT _ng_editor_type STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "ng_attach_editor_app accepts only the actual editor executable")
    endif()
    cmake_parse_arguments(NG_APP "HAVE_MCP;IN_NIGHTLY;REGRESSION_SMOKE;IN_LITE" "" "" ${ARGN})
    if(NG_APP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown ng_attach_editor_app option: ${NG_APP_UNPARSED_ARGUMENTS}")
    endif()
    target_sources("${target}" PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/app.cpp")
    target_sources("${target}" PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/app.hpp")
    target_include_directories("${target}" PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src")
    target_compile_features("${target}" PRIVATE cxx_std_20)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/agent_panel.cmake")
    foreach(_ng_editor_panel nijigenerate_animation_list_panel nijigenerate_armed_parameter_panel
            nijigenerate_autorig_panel_registration nijigenerate_agent_panel
            nijigenerate_action_history_panel)
        if(NOT TARGET "${_ng_editor_panel}")
            message(FATAL_ERROR "ng_attach_editor_app requires the actual ${_ng_editor_panel} provider target")
        endif()
        target_link_libraries("${target}" PRIVATE "${_ng_editor_panel}")
    endforeach()
    if(NOT TARGET nijigenerate_vertex_viewport)
        message(FATAL_ERROR
            "ng_attach_editor_app requires the actual nijigenerate_vertex_viewport provider target")
    endif()
    target_link_libraries("${target}" PRIVATE nijigenerate_vertex_viewport)
    target_compile_definitions("${target}" PRIVATE "$<$<CONFIG:Debug>:NG_DEBUG>")
    foreach(_ng_editor_feature HAVE_MCP IN_NIGHTLY REGRESSION_SMOKE)
        if(NG_APP_${_ng_editor_feature})
            target_compile_definitions("${target}" PRIVATE "NG_${_ng_editor_feature}")
        endif()
    endforeach()
    # The selected native Unicode Windows entry is wmain. Keep the source's
    # release/non-lite GUI-subsystem gate; Debug/lite use the console CRT.
    if(WIN32 AND MSVC AND NOT NG_APP_IN_LITE)
        target_link_options("${target}" PRIVATE
            "$<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>"
            "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:wmainCRTStartup>")
    endif()
endfunction()
