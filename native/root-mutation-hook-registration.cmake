target_sources(nijigenerate_core PRIVATE src/nijigenerate/actions/depthbone_mutation_hook.cpp)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_depthbone_mutation_hook_probe
        tests/depthbone_mutation_hook_probe.cpp tests/depthbone_mutation_hook_other_tu.cpp)
    target_link_libraries(nijigenerate_depthbone_mutation_hook_probe PRIVATE nijigenerate_core Threads::Threads)
    add_test(NAME native_depthbone_mutation_hook_storage COMMAND nijigenerate_depthbone_mutation_hook_probe)
    set(NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC "" CACHE FILEPATH
        "Existing independent D dependency source spec for full-module declaration checks")
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON
            AND EXISTS "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}")
        add_test(NAME native_depthbone_mutation_hook_declarations
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mutation_hook_declarations.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthbone_mutation_hook_probe>"
                --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/mutation-hook-declarations/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()
