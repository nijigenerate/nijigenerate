if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_automesh_dynamic_progress)
    find_package(Threads REQUIRED)
    # Shared progress state used by the real worker and main-thread notification.
    add_library(nijigenerate_automesh_dynamic_progress STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_progress.cpp")
    target_include_directories(nijigenerate_automesh_dynamic_progress PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_dynamic_progress PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_dynamic_progress PUBLIC nijigenerate_core Threads::Threads)
endif()
