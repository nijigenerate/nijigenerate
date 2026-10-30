# One genuine fixed-pattern matcher provider, shared by its actual consumers.
if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_autorig_regex)
    add_library(nijigenerate_autorig_regex STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/physics_regex.cpp")
    target_compile_features(nijigenerate_autorig_regex PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_autorig_regex PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_autorig_regex PUBLIC nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_regex PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_regex PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
