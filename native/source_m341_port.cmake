# The existing welding.d entry point only; no generated harness or new cases.
if(TARGET nijigenerate_core AND TARGET nicxlive::nicxlive
        AND TARGET nijigenerate_phobos_stdio AND TARGET nijigenerate_gl_backend_source)
    add_library(nijigenerate_source_m341 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate_tests/welding.cpp")
    target_sources(nijigenerate_source_m341 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate_tests/welding.hpp")
    target_include_directories(nijigenerate_source_m341 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_source_m341 PUBLIC nijigenerate_core nicxlive::nicxlive
        nijigenerate_phobos_stdio nijigenerate_gl_backend_source)
    target_compile_features(nijigenerate_source_m341 PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_source_m341 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m341 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
