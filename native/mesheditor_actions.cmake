if(TARGET nijigenerate_mesh_editor_tools AND NOT TARGET nijigenerate_mesheditor_actions)
    # mesheditor.cpp and its explicit template instantiations belong to this provider.
    add_library(nijigenerate_mesheditor_actions INTERFACE)
    target_include_directories(nijigenerate_mesheditor_actions INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_mesheditor_actions INTERFACE cxx_std_20)
    target_sources(nijigenerate_mesheditor_actions INTERFACE
        FILE_SET mesheditor_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesheditor.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesheditor_action_templates.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesheditor_path_templates.hpp")
    target_link_libraries(nijigenerate_mesheditor_actions INTERFACE nijigenerate_mesh_editor_tools)
endif()
