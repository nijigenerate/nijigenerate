if(TARGET nijigenerate_runtime_core)
    set(puppet_file_complete_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_constructors.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_runtime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_merge.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_video.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_registry.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${puppet_file_complete_sources})
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET puppet_file_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/file_commands.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_imgexport_window nijigenerate_videoexport_window
        nijigenerate_psdmerge_window nijigenerate_kramerge_window
        nijigenerate_imageexport nijigenerate_io)
    if(MSVC)
        set_source_files_properties(${puppet_file_complete_sources} PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        set_source_files_properties(${puppet_file_complete_sources} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
