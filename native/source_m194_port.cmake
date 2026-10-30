# Existing version(RegressionSmoke) original only; no runner or generated cases.
option(NIJIGENERATE_REGRESSION_SMOKE "Publish the original RegressionSmoke module" OFF)
if(NIJIGENERATE_REGRESSION_SMOKE AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_windows AND TARGET nicxlive::nicxlive)
    # The same gate must select the defining PSD dialog methods and their consumers.
    add_compile_definitions(NG_REGRESSION_SMOKE)
    target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_REGRESSION_SMOKE)
    add_library(nijigenerate_source_m194 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/regression_smoke.cpp")
    target_sources(nijigenerate_source_m194 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/regression_smoke.hpp")
    target_include_directories(nijigenerate_source_m194 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_definitions(nijigenerate_source_m194 PUBLIC NG_REGRESSION_SMOKE)
    target_compile_features(nijigenerate_source_m194 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m194 PUBLIC
        nijigenerate_runtime_core nijigenerate_windows nicxlive::nicxlive)
    if(TARGET nijigenerate)
        # The existing app.cpp version branch imports these actual entry points.
        target_link_libraries(nijigenerate PRIVATE nijigenerate_source_m194)
    endif()
    if(MSVC)
        target_compile_options(nijigenerate_source_m194 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m194 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
