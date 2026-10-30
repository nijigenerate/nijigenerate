if(TARGET nijigenerate_autorig_framework AND TARGET nijigenerate_core)
    add_library(nijigenerate_autorig_observation STATIC
        src/nijigenerate/autorig/deterministic/observation_sampling.cpp
        src/nijigenerate/autorig/deterministic/observation_contour.cpp
        src/nijigenerate/autorig/deterministic/observation_topology.cpp
        src/nijigenerate/autorig/deterministic/observation_shoulders.cpp)
    target_compile_features(nijigenerate_autorig_observation PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_observation
        PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/observation.hpp"
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/detail/observation.hpp")
    target_link_libraries(nijigenerate_autorig_observation
        PUBLIC nijigenerate_autorig_framework nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_observation PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_observation PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
