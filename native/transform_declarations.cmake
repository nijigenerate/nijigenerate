if(TARGET nicxlive::nicxlive)
    add_library(nijigenerate_transform_declarations INTERFACE)
    target_compile_features(nijigenerate_transform_declarations INTERFACE cxx_std_20)
    target_sources(nijigenerate_transform_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/utils/transform.hpp")
    target_include_directories(nijigenerate_transform_declarations INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/third_party")
    target_link_libraries(nijigenerate_transform_declarations INTERFACE nijigenerate_native_options Boost::json)
    if(BUILD_TESTING)
        add_library(nijigenerate_transform_header_probe OBJECT
            tests/transform_header_a.cpp tests/transform_header_b.cpp)
        target_link_libraries(nijigenerate_transform_header_probe PRIVATE nijigenerate_transform_declarations)
        add_custom_target(nijigenerate_transform_header_project DEPENDS nijigenerate_transform_header_probe)
    endif()
endif()
