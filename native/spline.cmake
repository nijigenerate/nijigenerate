if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_debug
        AND TARGET nijigenerate_viewport_mesh AND NOT TARGET nijigenerate_spline)
    add_library(nijigenerate_spline STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/spline.cpp")
    target_include_directories(nijigenerate_spline PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_spline PUBLIC
        FILE_SET spline_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/spline.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_spline.hpp")
    target_compile_features(nijigenerate_spline PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_spline PUBLIC nijigenerate_runtime_core nijigenerate_debug
        nijigenerate_viewport_mesh)
    # Public spline templates also execute finite guards and ordered arithmetic.
    if(MSVC)
        target_compile_options(nijigenerate_spline PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_spline PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
