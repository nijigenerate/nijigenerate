# Complete independent psd-d provider, backed by the installed zlib runtime.
if(TARGET nijigenerate_core)
    find_package(ZLIB REQUIRED)
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/layer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/parser.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/parser_document.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/rle.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/uncompress.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/uncompress.hpp")
    target_sources(nijigenerate_core PUBLIC FILE_SET psd_rle_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/rle.hpp")
    target_link_libraries(nijigenerate_core PRIVATE ZLIB::ZLIB)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/parser.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/parser.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
