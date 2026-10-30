# Complete M152 generic bodies and M157 PSD specialization; real project I/O is required.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_io)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET import_layer_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport_fwd.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport_layers.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_io)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inimport.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
