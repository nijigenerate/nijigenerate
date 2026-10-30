if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imagefmt_writer)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_content.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET psd_dialog_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_commands.hpp")
    target_link_libraries(nijigenerate_runtime_core PRIVATE nijigenerate_imagefmt_writer)
    if(MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_commands.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_content.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_commands.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_content.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
