if(TARGET nijigenerate_autorig_observation AND NOT TARGET nijigenerate_autorig_registered)
    add_library(nijigenerate_autorig_registered STATIC
        src/nijigenerate/autorig/deterministic/registered_geometry.cpp
        src/nijigenerate/autorig/deterministic/registered.cpp
        src/nijigenerate/autorig/deterministic/detail/registered.hpp
        src/nijigenerate/autorig/deterministic/detail/registered_resource.hpp
        src/nijigenerate/autorig/deterministic/detail/sha256.hpp)
    target_include_directories(nijigenerate_autorig_registered PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_registered PUBLIC
        FILE_SET autorig_registered_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/registered.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/registered_landmarks.hpp")
    target_compile_features(nijigenerate_autorig_registered PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_registered PUBLIC nijigenerate_autorig_observation)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_registered PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_registered PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
