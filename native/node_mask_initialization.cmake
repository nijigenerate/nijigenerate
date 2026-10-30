if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_core
        AND TARGET nicxlive::nicxlive AND NOT TARGET nijigenerate_node_mask_initialization)
    add_library(nijigenerate_node_mask_initialization STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/mask_initialization.cpp")
    target_include_directories(nijigenerate_node_mask_initialization PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_node_mask_initialization PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_node_mask_initialization PUBLIC
        nijigenerate_runtime_core nijigenerate_core nicxlive::nicxlive)
endif()
