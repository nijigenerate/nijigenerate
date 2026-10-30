if(TARGET nijigenerate_imgui)
    add_library(nijigenerate_widgets_declarations INTERFACE)
    target_compile_features(nijigenerate_widgets_declarations INTERFACE cxx_std_20)
    target_sources(nijigenerate_widgets_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/window.hpp")
    target_include_directories(nijigenerate_widgets_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_widgets_declarations INTERFACE
        nijigenerate_native_options nijigenerate_imgui)
    if(BUILD_TESTING)
        add_executable(nijigenerate_widgets_public_probe tests/widgets_public_a.cpp tests/widgets_public_b.cpp)
        target_link_libraries(nijigenerate_widgets_public_probe PRIVATE nijigenerate_widgets_declarations)
        add_custom_target(nijigenerate_widgets_public_project DEPENDS nijigenerate_widgets_public_probe)
        add_test(NAME native_widgets_public_contract COMMAND nijigenerate_widgets_public_probe)
    endif()
endif()
