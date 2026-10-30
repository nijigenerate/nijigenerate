# Actual editor product. Module bodies and startup remain in their existing
# publishers; this file supplies the native entry and executable composition.
include_guard(GLOBAL)
if(NOT NIJIGENERATE_BUILD_EDITOR)
    return()
endif()

set(_ng_editor_providers
    nijigenerate_native_options
    nijigenerate_runtime_core
    nijigenerate_core_package
    nijigenerate_viewport_package
    nijigenerate_mesh_editor_tools
    nijigenerate::windows
    nijigenerate_flipconfig
    nijigenerate_animation_list_panel
    nijigenerate_armed_parameter_panel
    nijigenerate_vertex_viewport
    SDL3::SDL3
    nicxlive::nicxlive)
foreach(_ng_editor_provider IN LISTS _ng_editor_providers)
    if(NOT TARGET "${_ng_editor_provider}")
        message(FATAL_ERROR
            "The editor requires its actual ${_ng_editor_provider} provider target")
    endif()
endforeach()

add_executable(nijigenerate "${CMAKE_CURRENT_LIST_DIR}/src/editor_entry.cpp")
target_link_libraries(nijigenerate PRIVATE ${_ng_editor_providers})
set(_ng_editor_features)
foreach(_ng_editor_feature HAVE_MCP IN_NIGHTLY IN_LITE)
    if(NIJIGENERATE_EDITOR_${_ng_editor_feature})
        list(APPEND _ng_editor_features "${_ng_editor_feature}")
    endif()
endforeach()
ng_attach_editor_app(nijigenerate ${_ng_editor_features})

# MSVC's selected Unicode CRT policy is supplied by the existing app publisher.
# MinGW needs its corresponding wide-argument startup selection explicitly.
if(MINGW)
    target_link_options(nijigenerate PRIVATE -municode)
    if(NOT NIJIGENERATE_EDITOR_IN_LITE)
        target_link_options(nijigenerate PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-mwindows>")
    endif()
endif()
