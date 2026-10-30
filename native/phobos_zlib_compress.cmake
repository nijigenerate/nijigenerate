if(TARGET nijigenerate_core AND TARGET ZLIB::ZLIB)
    target_sources(nijigenerate_core PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_zlib_compress.hpp")
    target_link_libraries(nijigenerate_core PUBLIC ZLIB::ZLIB)
endif()
