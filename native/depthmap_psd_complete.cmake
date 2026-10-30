# Full PSD depth import and composition implementation; validation follows source completion.
option(NIJIGENERATE_COMMAND_BROWSER_DIFFERENTIAL "Map the D CommandBrowserDifferential source version" OFF)
if(TARGET nijigenerate_runtime_core)
    if(NIJIGENERATE_COMMAND_BROWSER_DIFFERENTIAL)
        target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_COMMAND_BROWSER_DIFFERENTIAL=1)
    endif()
    set(_depthmap_psd_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_composition.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_helpers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_targets.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_matching.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_loading.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_sampling.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_build.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_image.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_dispatch.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_depthmap_psd_sources})
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/detail/psd_pixel_copy.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_values.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_values_fwd.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_composite_aliases.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_names.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_enum_functions.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_math.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_composition.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_public_targets.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_public_sampling.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_import.hpp")
    if(MSVC)
        set_property(SOURCE ${_depthmap_psd_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_depthmap_psd_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_depthmap_psd_sources)
endif()
