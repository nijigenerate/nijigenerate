if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_autorig_anatomy)
    add_library(nijigenerate_autorig_anatomy STATIC
        src/nijigenerate/autorig/deterministic/anatomy.cpp
        src/nijigenerate/autorig/deterministic/anatomy_scaffold.cpp
        src/nijigenerate/autorig/deterministic/anatomy_depth.cpp)
    target_compile_features(nijigenerate_autorig_anatomy PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_autorig_anatomy PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_anatomy PUBLIC
        FILE_SET anatomy_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/anatomy.hpp")
    target_link_libraries(nijigenerate_autorig_anatomy PUBLIC
        nijigenerate_runtime_core nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_anatomy PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_anatomy PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
