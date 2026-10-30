add_library(nijigenerate_timeline_declarations INTERFACE)
target_include_directories(nijigenerate_timeline_declarations INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(nijigenerate_timeline_declarations INTERFACE nijigenerate_native_options)
target_sources(nijigenerate_timeline_declarations INTERFACE
    FILE_SET timeline_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/timeline.hpp")
