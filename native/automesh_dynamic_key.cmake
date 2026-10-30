if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_automesh_dynamic_key)
    add_library(nijigenerate_automesh_dynamic_key STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_key.cpp")
    target_include_directories(nijigenerate_automesh_dynamic_key PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic_key PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic_key PUBLIC nijigenerate_core)
endif()
