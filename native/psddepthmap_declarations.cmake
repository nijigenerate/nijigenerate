# The opaque public boundary does not supply the dialog class or GPU bodies.
add_library(nijigenerate_psddepthmap_declarations INTERFACE)
target_link_libraries(nijigenerate_psddepthmap_declarations INTERFACE nijigenerate_native_options)
target_include_directories(nijigenerate_psddepthmap_declarations INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(nijigenerate_psddepthmap_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap.hpp")
