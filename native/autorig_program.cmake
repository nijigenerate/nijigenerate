if(TARGET nijigenerate_autorig_evidence AND TARGET nijigenerate_autorig_hierarchy AND TARGET nijigenerate_autorig_anatomy AND NOT TARGET nijigenerate_autorig_program)
    add_library(nijigenerate_autorig_program STATIC src/nijigenerate/autorig/deterministic/program.cpp)
    target_compile_features(nijigenerate_autorig_program PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_program PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/program.hpp")
    target_link_libraries(nijigenerate_autorig_program PUBLIC nijigenerate_autorig_evidence nijigenerate_autorig_hierarchy nijigenerate_autorig_anatomy)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_program PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_program PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
