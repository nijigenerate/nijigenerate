if(NOT TARGET nijigenerate_viewport_common AND
    TARGET nijigenerate_native_options AND TARGET nicxlive::nicxlive)
    add_library(nijigenerate_viewport_common STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/package.cpp")
    target_include_directories(nijigenerate_viewport_common PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_viewport_common PUBLIC
        FILE_SET viewport_common_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/package.hpp")
    target_compile_features(nijigenerate_viewport_common PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_viewport_common PUBLIC nijigenerate_native_options nicxlive::nicxlive)
    if(MSVC)
        target_compile_options(nijigenerate_viewport_common PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_viewport_common PRIVATE -ffp-contract=off)
    endif()
endif()
