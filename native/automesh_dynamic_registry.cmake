if(TARGET nijigenerate_core AND TARGET nijigenerate_automesh_dynamic_key
        AND NOT TARGET nijigenerate_automesh_dynamic_registry)
    add_library(nijigenerate_automesh_dynamic_registry STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_registry.cpp")
    target_include_directories(nijigenerate_automesh_dynamic_registry PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic_registry PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic_registry PUBLIC
        nijigenerate_core nijigenerate_automesh_dynamic_key)
endif()
