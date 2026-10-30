if(TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_actions_depthbone STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone.cpp")
    target_sources(nijigenerate_actions_depthbone PUBLIC
        FILE_SET actions_depthbone_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depthbone.hpp")
    target_include_directories(nijigenerate_actions_depthbone PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_actions_depthbone PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_actions_depthbone PUBLIC
        nijigenerate_runtime_core nijigenerate_depthbone_nodes)
endif()
