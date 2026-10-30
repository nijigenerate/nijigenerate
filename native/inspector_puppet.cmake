if(TARGET nijigenerate_inspector_common AND TARGET nijigenerate_inputtext)
    add_library(nijigenerate_inspector_puppet STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/puppet.cpp")
    target_sources(nijigenerate_inspector_puppet PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/puppet.hpp")
    target_link_libraries(nijigenerate_inspector_puppet PUBLIC nijigenerate_inspector_common
        PRIVATE nijigenerate_inputtext nijigenerate_category nijigenerate_label nijigenerate_dummy
        nijigenerate_tooltip nijigenerate_drag nijigenerate_button)
    if(MSVC)
        target_compile_options(nijigenerate_inspector_puppet PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_inspector_puppet PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
