if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_automesh_dynamic_logging)
    add_library(nijigenerate_automesh_dynamic_logging INTERFACE)
    target_include_directories(nijigenerate_automesh_dynamic_logging INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic_logging INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic_logging INTERFACE nijigenerate_core)
endif()
