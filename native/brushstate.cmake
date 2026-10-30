if(NIJIGENERATE_RUNTIME_PROBE AND TARGET nijigenerate_window_theme)
    add_library(nijigenerate_window_background STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_background.cpp")
    target_include_directories(nijigenerate_window_background PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_window_background PUBLIC cxx_std_20)
    target_sources(nijigenerate_window_background PRIVATE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_background_state.hpp")
    target_link_libraries(nijigenerate_window_background PUBLIC
        nijigenerate_window_declarations nijigenerate_window_theme nicxlive::nicxlive)
    add_library(nijigenerate_brushstate STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushstate.cpp")
    target_include_directories(nijigenerate_brushstate PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_brushstate PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_brushstate PUBLIC nijigenerate_window_background)
endif()
