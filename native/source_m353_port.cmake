# Existing tests/autorig_solver.d only; no generated cases or test execution.
if(TARGET nijigenerate_autorig_processor AND TARGET nijigenerate_autorig_controls
        AND TARGET nijigenerate_autorig_workflow AND TARGET nijigenerate_phobos_stdio
        AND TARGET nijigenerate_source_m352 AND NOT TARGET nijigenerate_source_m353)
    add_library(nijigenerate_source_m353 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_solver.cpp")
    target_sources(nijigenerate_source_m353 PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_solver.hpp")
    target_include_directories(nijigenerate_source_m353 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_compile_features(nijigenerate_source_m353 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m353 PUBLIC nijigenerate_autorig_processor
        nijigenerate_autorig_controls nijigenerate_autorig_workflow nijigenerate_phobos_stdio
        nijigenerate_source_m352)
    if(MSVC)
        target_compile_options(nijigenerate_source_m353 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m353 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_autorig_solver_source
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_solver_main.cpp")
    target_link_libraries(nijigenerate_autorig_solver_source PRIVATE nijigenerate_source_m353)
    if(MINGW)
        target_link_options(nijigenerate_autorig_solver_source PRIVATE -municode)
    endif()
endif()
