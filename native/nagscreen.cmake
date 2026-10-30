if(TARGET nijigenerate_markdown_declarations AND TARGET nijigenerate_markdown
        AND TARGET nijigenerate_imgui AND NOT TARGET nijigenerate_nagscreen)
add_library(nijigenerate_nagscreen_declarations INTERFACE)
target_include_directories(nijigenerate_nagscreen_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_nagscreen_declarations INTERFACE cxx_std_20)
target_sources(nijigenerate_nagscreen_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal/nagscreen.hpp")
target_link_libraries(nijigenerate_nagscreen_declarations INTERFACE
    nijigenerate_markdown_declarations nijigenerate_imgui nijigenerate_native_options)
add_library(nijigenerate_nagscreen STATIC src/nijigenerate/widgets/modal/nagscreen_hooks.cpp)
target_link_libraries(nijigenerate_nagscreen PUBLIC
    nijigenerate_nagscreen_declarations nijigenerate_markdown)
if(BUILD_TESTING AND TARGET nijigenerate_imgui)
    add_library(nijigenerate_nagscreen_header_probe OBJECT tests/nagscreen_header_probe.cpp)
    target_link_libraries(nijigenerate_nagscreen_header_probe PRIVATE nijigenerate_nagscreen_declarations)
    add_custom_target(nijigenerate_nagscreen_project DEPENDS
        nijigenerate_nagscreen_header_probe nijigenerate_nagscreen)
endif()
endif()
