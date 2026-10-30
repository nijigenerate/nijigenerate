if(NIJIGENERATE_IMGUI AND NOT TARGET nijigenerate_viewport_base_primitives)
    # The real base/delegation/state bodies; MainViewport remains a separate gate.
    add_library(nijigenerate_viewport_base_primitives STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_object.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_delegation.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base_state.cpp")
    target_sources(nijigenerate_viewport_base_primitives PUBLIC
        FILE_SET viewport_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/base.hpp")
    target_include_directories(nijigenerate_viewport_base_primitives PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_viewport_base_primitives PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_viewport_base_primitives PUBLIC
        nijigenerate_native_options nijigenerate_imgui_sdl3 nicxlive::nicxlive)
endif()
