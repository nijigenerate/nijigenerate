# Self-contained canonical registration; safe to include in CPU-only projects.
target_sources(nijigenerate_core PUBLIC
    FILE_SET depthbone_mutation_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone_mutation_kind.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone_mutation.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone_mutation_hook.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone_publishers.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone_publisher_assertion.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthboneinvalidation.hpp")
if(BUILD_TESTING)
add_library(nijigenerate_depthbone_module_declarations OBJECT
    tests/depthbone_module_declarations.cpp)
target_link_libraries(nijigenerate_depthbone_module_declarations PRIVATE nijigenerate_core)
endif()
if(BUILD_TESTING AND NIJIGENERATE_RUNTIME_PROBE)
add_executable(nijigenerate_depthbone_mutation_storage_probe
    tests/depthbone_mutation_storage_probe.cpp)
target_link_libraries(nijigenerate_depthbone_mutation_storage_probe PRIVATE
    nijigenerate_core nicxlive::nicxlive)
if(WIN32)
    add_custom_command(TARGET nijigenerate_depthbone_mutation_storage_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:nijigenerate_depthbone_mutation_storage_probe>
            $<TARGET_FILE_DIR:nijigenerate_depthbone_mutation_storage_probe>
        COMMAND_EXPAND_LISTS)
endif()
add_test(NAME native_depthbone_mutation_storage
    COMMAND nijigenerate_depthbone_mutation_storage_probe)
add_executable(nijigenerate_depthbone_mutation_runtime_probe
    tests/depthbone_mutation_runtime_probe.cpp)
target_link_libraries(nijigenerate_depthbone_mutation_runtime_probe PRIVATE
    nijigenerate_core nicxlive::nicxlive)
if(WIN32)
    add_custom_command(TARGET nijigenerate_depthbone_mutation_runtime_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:nijigenerate_depthbone_mutation_runtime_probe>
            $<TARGET_FILE_DIR:nijigenerate_depthbone_mutation_runtime_probe>
        COMMAND_EXPAND_LISTS)
endif()
add_test(NAME native_depthbone_mutation_runtime_ownership
    COMMAND nijigenerate_depthbone_mutation_runtime_probe)
if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON
        AND EXISTS "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}")
    add_test(NAME native_depthbone_mutation_record_semantics
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mutation_record_semantics.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --native "$<TARGET_FILE:nijigenerate_depthbone_mutation_storage_probe>"
            --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/mutation-record-semantics/$<CONFIG>"
            --configuration "$<CONFIG>")
    add_test(NAME native_depthbone_mutation_record_differential
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_mutation_record_runtime.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --native "$<TARGET_FILE:nijigenerate_depthbone_mutation_runtime_probe>"
            --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/mutation-record-runtime/$<CONFIG>"
            --configuration "$<CONFIG>")
endif()
endif()
