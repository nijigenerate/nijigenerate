if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PUBLIC
        FILE_SET model_mesheditor_node_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/node.hpp")
endif()

if(TARGET nijigenerate_core AND BUILD_TESTING)
    add_executable(nijigenerate_model_mesheditor_node_probe "${CMAKE_CURRENT_LIST_DIR}/tests/model_mesheditor_node_probe.cpp")
    target_link_libraries(nijigenerate_model_mesheditor_node_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_model_mesheditor_node_declaration
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_model_mesheditor_node.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_model_mesheditor_node_probe>" --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/model-mesheditor-node-evidence/$<CONFIG>")
    endif()
endif()
