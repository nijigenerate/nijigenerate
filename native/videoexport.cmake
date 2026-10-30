if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport.cpp")
    if(WIN32)
        target_sources(nijigenerate_runtime_core PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport_process.cpp")
    elseif(UNIX)
        target_sources(nijigenerate_runtime_core PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport_process_posix.cpp")
        if(CMAKE_DL_LIBS)
            target_link_libraries(nijigenerate_runtime_core PRIVATE ${CMAKE_DL_LIBS})
        endif()
    else()
        message(FATAL_ERROR "Video export requires an actual platform process provider")
    endif()
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET videoexport_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_execute_shell.hpp")
    target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/videoexport.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
    endif()
endif()
