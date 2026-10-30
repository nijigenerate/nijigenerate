if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_autorig_depth)
    add_library(nijigenerate_autorig_surface STATIC src/nijigenerate/autorig/deterministic/surface.cpp)
    target_compile_features(nijigenerate_autorig_surface PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_surface PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/surface.hpp")
    # Geometry lives in the real core/runtime targets; templates in runtime_core.
    # The contracts target is declared later by the normal module publication list.
    target_link_libraries(nijigenerate_autorig_surface PUBLIC nijigenerate_autorig_depth
        nijigenerate_runtime_core nijigenerate_core nijigenerate_autorig_contracts)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_surface PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_surface PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
