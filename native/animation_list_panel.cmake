if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels AND TARGET nijigenerate_editanim_window
        AND TARGET nijigenerate_button AND TARGET nijigenerate_dummy
        AND NOT TARGET nijigenerate_animation_list_panel)
    add_library(nijigenerate_animation_list_panel STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/animlist.cpp")
    target_sources(nijigenerate_animation_list_panel PUBLIC
        FILE_SET animation_list_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/animlist.hpp")
    target_compile_features(nijigenerate_animation_list_panel PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_animation_list_panel PUBLIC nijigenerate_panels
        PRIVATE nijigenerate_runtime_core nijigenerate_editanim_window nijigenerate_button nijigenerate_dummy)
endif()
