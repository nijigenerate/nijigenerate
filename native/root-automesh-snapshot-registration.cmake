target_sources(nijigenerate_core PRIVATE
    src/nijigenerate/compat/retained_string_slice.hpp
    src/nijigenerate/commands/automesh/detail/config_snapshot.hpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_automesh_snapshot_storage_probe tests/automesh_snapshot_storage_probe.cpp)
    target_link_libraries(nijigenerate_automesh_snapshot_storage_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_automesh_snapshot_storage_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automesh_snapshot_storage.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_automesh_snapshot_storage_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-snapshot-storage-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()
