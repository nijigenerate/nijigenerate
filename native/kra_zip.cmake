if(TARGET nijigenerate_core AND TARGET ZLIB::ZLIB)
    target_sources(nijigenerate_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/zip.cpp")
    target_link_libraries(nijigenerate_core PRIVATE ZLIB::ZLIB)
endif()
