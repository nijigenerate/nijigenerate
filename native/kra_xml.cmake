if(TARGET nicxlive::common AND NOT TARGET nijigenerate_kra_xml)
    add_library(nijigenerate_kra_xml STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/xml.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/xml_graphical.hpp")
    target_include_directories(nijigenerate_kra_xml PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_kra_xml PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_kra_xml PUBLIC nicxlive::common)
    target_sources(nijigenerate_kra_xml PUBLIC FILE_SET kra_xml_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra/xml.hpp")
endif()
