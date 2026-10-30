if(TARGET nijigenerate_core AND TARGET nijigenerate_runtime_core
        AND NOT TARGET nijigenerate_automesh_dynamic_apply_environment)
    find_package(Threads REQUIRED)
    add_library(nijigenerate_automesh_dynamic_apply_environment STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_apply_environment.cpp")
    target_include_directories(nijigenerate_automesh_dynamic_apply_environment PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic_apply_environment PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic_apply_environment PUBLIC
        nijigenerate_core nijigenerate_runtime_core Threads::Threads)
endif()
