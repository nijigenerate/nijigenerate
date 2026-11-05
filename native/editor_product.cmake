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
    nijigenerate_settings
    nijigenerate_parameter_base
    nijigenerate_command_scope_json
    nijigenerate_vertex_define_commands
    nijigenerate_palette_state
    nijigenerate_window_finalize
    nijigenerate_window_theme_settings
    SDL3::SDL3
    nicxlive::nicxlive)
if(WIN32)
    list(APPEND _ng_editor_providers nijigenerate_window_open_win32)
elseif(APPLE)
    list(APPEND _ng_editor_providers nijigenerate_window_open_macos)
elseif(UNIX)
    list(APPEND _ng_editor_providers nijigenerate_window_open_linux)
endif()
foreach(_ng_editor_provider IN LISTS _ng_editor_providers)
    if(NOT TARGET "${_ng_editor_provider}")
        message(FATAL_ERROR
            "The editor requires its actual ${_ng_editor_provider} provider target")
    endif()
endforeach()

add_executable(nijigenerate "${CMAKE_CURRENT_LIST_DIR}/src/editor_entry.cpp")
if(WIN32)
    enable_language(RC)
    set(_ng_editor_resource_dir "${PROJECT_SOURCE_DIR}/build-aux/windows")
    target_sources(nijigenerate PRIVATE "${_ng_editor_resource_dir}/nijigenerate.rc")
    target_include_directories(nijigenerate PRIVATE
        "$<$<COMPILE_LANGUAGE:RC>:${_ng_editor_resource_dir}>")
    set_property(SOURCE "${_ng_editor_resource_dir}/nijigenerate.rc" APPEND PROPERTY OBJECT_DEPENDS
        "${_ng_editor_resource_dir}/version.h"
        "${_ng_editor_resource_dir}/nijigenerate.ico")
endif()
target_link_libraries(nijigenerate PRIVATE ${_ng_editor_providers})
set(_ng_editor_features)
foreach(_ng_editor_feature HAVE_MCP IN_NIGHTLY IN_LITE)
    if(NIJIGENERATE_EDITOR_${_ng_editor_feature})
        list(APPEND _ng_editor_features "${_ng_editor_feature}")
    endif()
endforeach()
ng_attach_editor_app(nijigenerate ${_ng_editor_features})

if(WIN32)
    # FindZLIB publishes an UNKNOWN imported library, so TARGET_RUNTIME_DLLS
    # alone does not include its DLL. Inspect the linked executable as well.
    set(_ng_editor_runtime_dirs)
    foreach(_ng_zlib_library IN LISTS ZLIB_LIBRARIES)
        if(IS_ABSOLUTE "${_ng_zlib_library}")
            get_filename_component(_ng_zlib_dir "${_ng_zlib_library}" DIRECTORY)
            list(APPEND _ng_editor_runtime_dirs "${_ng_zlib_dir}/../bin")
        endif()
    endforeach()
    set(_ng_editor_dependency_scanner)
    if(MSVC)
        get_filename_component(_ng_linker_dir "${CMAKE_LINKER}" DIRECTORY)
        set(_ng_editor_dependency_scanner
            "-DCMAKE_GET_RUNTIME_DEPENDENCIES_TOOL=dumpbin"
            "-DCMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND=${_ng_linker_dir}/dumpbin.exe")
    endif()
    add_custom_command(TARGET nijigenerate POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DNG_EDITOR_EXECUTABLE=$<TARGET_FILE:nijigenerate>"
            "-DNG_EDITOR_RUNTIME_DLLS=$<TARGET_RUNTIME_DLLS:nijigenerate>"
            "-DNG_EDITOR_RUNTIME_DIRS=${_ng_editor_runtime_dirs}"
            ${_ng_editor_dependency_scanner}
            -P "${CMAKE_CURRENT_LIST_DIR}/deploy_editor_runtime.cmake"
        VERBATIM)
endif()

# MSVC's selected Unicode CRT policy is supplied by the existing app publisher.
# MinGW needs its corresponding wide-argument startup selection explicitly.
if(MINGW)
    target_link_options(nijigenerate PRIVATE -municode)
    if(NOT NIJIGENERATE_EDITOR_IN_LITE)
        target_link_options(nijigenerate PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-mwindows>")
    endif()
endif()
