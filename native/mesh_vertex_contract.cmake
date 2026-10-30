if(BUILD_TESTING AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR)
    add_test(NAME native_mesh_vertex_type_d_contract
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/observe_mesh_vertex_type.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/mesh-vertex-type-evidence/$<CONFIG>")
endif()
