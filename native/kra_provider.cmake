# Independent kra-d package/layer/parser/LZF sources; ZIP/XML are real sibling providers.
if(TARGET nijigenerate_core)
    target_link_libraries(nijigenerate_core PRIVATE nijigenerate_kra_xml)
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/layer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/lzf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/tiles.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/parser.cpp")
    target_sources(nijigenerate_core PUBLIC FILE_SET kra_provider_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/document.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/zip.hpp")
    if(TARGET nijigenerate_kra_xml)
        target_link_libraries(nijigenerate_core PRIVATE nijigenerate_kra_xml)
    endif()
endif()
