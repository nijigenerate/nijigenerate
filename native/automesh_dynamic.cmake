if(TARGET nijigenerate_automesh_dynamic_registry AND NOT TARGET nijigenerate_automesh_dynamic)
    add_library(nijigenerate_automesh_dynamic INTERFACE)
    target_include_directories(nijigenerate_automesh_dynamic INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic INTERFACE
        nijigenerate_automesh_dynamic_registry nijigenerate_core)
endif()
