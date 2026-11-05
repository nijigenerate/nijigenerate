# Deploy the linked editor's third-party DLLs without depending on the user's PATH.
if(NOT EXISTS "${NG_EDITOR_EXECUTABLE}")
    message(FATAL_ERROR "Native editor executable is missing: ${NG_EDITOR_EXECUTABLE}")
endif()
get_filename_component(_ng_output_dir "${NG_EDITOR_EXECUTABLE}" DIRECTORY)
foreach(_ng_dll IN LISTS NG_EDITOR_RUNTIME_DLLS)
    get_filename_component(_ng_dll_name "${_ng_dll}" NAME)
    file(COPY_FILE "${_ng_dll}" "${_ng_output_dir}/${_ng_dll_name}" ONLY_IF_DIFFERENT)
endforeach()

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${NG_EDITOR_EXECUTABLE}"
    DIRECTORIES ${NG_EDITOR_RUNTIME_DIRS}
    RESOLVED_DEPENDENCIES_VAR _ng_resolved
    UNRESOLVED_DEPENDENCIES_VAR _ng_unresolved
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES [[.*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*]])
if(_ng_unresolved)
    message(FATAL_ERROR "Native editor runtime DLLs are missing: ${_ng_unresolved}")
endif()
foreach(_ng_dll IN LISTS _ng_resolved)
    get_filename_component(_ng_dll_name "${_ng_dll}" NAME)
    if(NOT _ng_dll STREQUAL "${_ng_output_dir}/${_ng_dll_name}")
        file(COPY_FILE "${_ng_dll}" "${_ng_output_dir}/${_ng_dll_name}" ONLY_IF_DIFFERENT)
    endif()
endforeach()
