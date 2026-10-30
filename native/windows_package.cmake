include_guard(GLOBAL)

# Include after the genuine child-module registrations. This package has no
# translation unit or initializer: its complete source consists of public imports.
if(NOT NIJIGENERATE_IMGUI OR NOT TARGET nijigenerate_runtime_core)
    return()
endif()

set(_ng_windows_targets
    nijigenerate_window_base
    nijigenerate_about
    nijigenerate_texviewer
    nijigenerate_parameditor_window
    nijigenerate_paramsplit_window
    nijigenerate_psdmerge_window
    nijigenerate_kramerge_window
    nijigenerate_rename_window
    nijigenerate_imgexport_window
    nijigenerate_flipconfig
    nijigenerate_videoexport_window
    nijigenerate_editanim_window
    nijigenerate_automeshbatch_window)
foreach(_ng_windows_target IN LISTS _ng_windows_targets)
    if(NOT TARGET "${_ng_windows_target}")
        message(FATAL_ERROR "The complete windows package requires actual provider ${_ng_windows_target}")
    endif()
endforeach()

# These three modules put their genuine bodies directly in runtime_core, rather
# than having separate targets. Require their complete current source sets.
set(_ng_windows_runtime_sources
    psddepthmap_state.cpp
    psddepthmap_value_declarations.cpp
    psddepthmap_canvas.cpp
    psddepthmap_dialog.cpp
    psddepthmap_edit_state.cpp
    psddepthmap_gpu.cpp
    psddepthmap_mapping_ui.cpp
    psddepthmap_options.cpp
    psddepthmap_preview.cpp
    psddepthmap_regression.cpp
    psddepthmap_samples.cpp
    psddepthmap_window.cpp
    welcome.cpp
    command_browser_internal.cpp
    command_browser_state.cpp
    command_browser_resources.cpp
    command_browser_inputs.cpp
    command_browser_outputs.cpp
    command_browser_execute.cpp
    command_browser_window.cpp
    command_browser_reflection.cpp
    command_browser_result_names.cpp
    command_browser_differential.cpp)
get_target_property(_ng_windows_registered nijigenerate_runtime_core SOURCES)
set(_ng_windows_registered_paths)
foreach(_ng_windows_source IN LISTS _ng_windows_registered)
    get_filename_component(_ng_windows_absolute "${_ng_windows_source}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    file(TO_CMAKE_PATH "${_ng_windows_absolute}" _ng_windows_absolute)
    list(APPEND _ng_windows_registered_paths "${_ng_windows_absolute}")
endforeach()
foreach(_ng_windows_source IN LISTS _ng_windows_runtime_sources)
    set(_ng_windows_expected "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/${_ng_windows_source}")
    file(TO_CMAKE_PATH "${_ng_windows_expected}" _ng_windows_expected)
    if(NOT _ng_windows_expected IN_LIST _ng_windows_registered_paths)
        message(FATAL_ERROR "The complete windows package requires actual registered source ${_ng_windows_source}")
    endif()
endforeach()

add_library(nijigenerate_windows INTERFACE)
add_library(nijigenerate::windows ALIAS nijigenerate_windows)
target_compile_features(nijigenerate_windows INTERFACE cxx_std_20)
target_include_directories(nijigenerate_windows INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(nijigenerate_windows INTERFACE FILE_SET windows_package_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/package.hpp")
target_link_libraries(nijigenerate_windows INTERFACE
    nijigenerate_runtime_core ${_ng_windows_targets})

unset(_ng_windows_targets)
unset(_ng_windows_target)
unset(_ng_windows_runtime_sources)
unset(_ng_windows_registered)
unset(_ng_windows_registered_paths)
unset(_ng_windows_source)
unset(_ng_windows_absolute)
unset(_ng_windows_expected)
