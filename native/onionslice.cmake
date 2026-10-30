if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/onionslice.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET onion_slice_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/onionslice.hpp")
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/onionslice.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/onionslice.cpp"
            PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
    endif()
endif()
