if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_viewport_base_primitives AND
        NOT TARGET nijigenerate_main_viewport)
    add_library(nijigenerate_main_viewport STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/main_viewport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_controls.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_transform_handle.cpp")
    target_sources(nijigenerate_main_viewport PUBLIC
        FILE_SET main_viewport_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/main_viewport.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_controls_internal.hpp")
    target_include_directories(nijigenerate_main_viewport PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_main_viewport PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_main_viewport PUBLIC nijigenerate_runtime_core
        nijigenerate_viewport_base_primitives nijigenerate_viewport_common
        PRIVATE nijigenerate_button nijigenerate_viewport nijigenerate_input
        nijigenerate_label nijigenerate_tooltip nijigenerate_transformhandle)
    if(MSVC)
        target_compile_options(nijigenerate_main_viewport PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_main_viewport PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
