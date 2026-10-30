if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels
        AND NOT TARGET nijigenerate_armed_parameter_panel)
    add_library(nijigenerate_armed_parameter_panel STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/armedparam.cpp")
    target_sources(nijigenerate_armed_parameter_panel PUBLIC
        FILE_SET armed_parameter_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/armedparam.hpp")
    target_compile_features(nijigenerate_armed_parameter_panel PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_armed_parameter_panel PUBLIC nijigenerate_panels
        PRIVATE nijigenerate_runtime_core)
endif()
