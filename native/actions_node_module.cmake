if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET node_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_value_action.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_hierarchy_actions.hpp")
endif()
if(BUILD_TESTING)
    add_executable(nijigenerate_actions_node_module_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/actions_node_module_probe.cpp")
    target_include_directories(nijigenerate_actions_node_module_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_actions_node_module_probe PRIVATE nijigenerate_native_options)
    target_compile_features(nijigenerate_actions_node_module_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_actions_node_module_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        get_filename_component(actions_node_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        get_filename_component(actions_node_compiler_root "${actions_node_compiler_bin}" DIRECTORY)
        add_test(NAME native_actions_node_module_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_actions_node_module.py"
                --native "$<TARGET_FILE:nijigenerate_actions_node_module_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/actions-node-module-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${actions_node_compiler_root}/import")
    endif()
endif()
