if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imagefmt_reader)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/project_reader.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/project_reader.hpp")
    target_include_directories(nijigenerate_runtime_core PRIVATE "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_imagefmt_reader)
endif()
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_open.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/autosave.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/autosave.hpp")
    target_include_directories(nijigenerate_runtime_core PRIVATE "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_window_base nijigenerate_label nijigenerate_button nijigenerate_dummy
        nijigenerate_imagefmt_reader)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_open.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/autosave.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    endif()
endif()
