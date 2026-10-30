if(TARGET nicxlive::nicxlive)
    add_library(nijigenerate_repair_declarations INTERFACE)
    target_include_directories(nijigenerate_repair_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_repair_declarations INTERFACE nijigenerate_native_options)
    target_sources(nijigenerate_repair_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/utils/repair.hpp")
    add_library(nijigenerate_repair_ids STATIC src/nijigenerate/utils/repair_ids.cpp)
    target_link_libraries(nijigenerate_repair_ids PUBLIC
        nijigenerate_repair_declarations nicxlive::nicxlive)
    if(BUILD_TESTING)
        add_library(nijigenerate_repair_header_probe OBJECT tests/repair_header_a.cpp tests/repair_header_b.cpp)
        target_link_libraries(nijigenerate_repair_header_probe PRIVATE nijigenerate_repair_declarations Boost::json)
        target_include_directories(nijigenerate_repair_header_probe PRIVATE
            "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core"
            "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/third_party")
        add_custom_target(nijigenerate_repair_header_project DEPENDS nijigenerate_repair_header_probe)
    endif()
endif()
