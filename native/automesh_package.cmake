if(TARGET nijigenerate_automesh_processor)
    target_sources(nijigenerate_automesh_processor PUBLIC
        FILE_SET automesh_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/type_sequence.hpp")
endif()

# Public imports bring the real providers to consumers of the package target.
# The facade has no translation unit, objects, factories or initialization.
if(TARGET nijigenerate_automesh_processor AND TARGET nijigenerate_automesh_contours
        AND TARGET nijigenerate_automesh_grid AND TARGET nijigenerate_automesh_skeleton
        AND TARGET nijigenerate_automesh_optimum AND TARGET nijigenerate_automesh_alpha_provider)
    add_library(nijigenerate_automesh INTERFACE)
    target_link_libraries(nijigenerate_automesh INTERFACE
        nijigenerate_automesh_processor nijigenerate_automesh_contours
        nijigenerate_automesh_grid nijigenerate_automesh_skeleton
        nijigenerate_automesh_optimum nijigenerate_automesh_alpha_provider)
endif()
