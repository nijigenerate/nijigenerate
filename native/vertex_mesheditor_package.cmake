# The original module only publicly imports its actual four editor modules.
# Publish on the existing aggregate that supplies those real implementations.
if(TARGET nijigenerate_vertex_editor)
    target_sources(nijigenerate_vertex_editor PUBLIC
        FILE_SET vertex_mesheditor_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/package.hpp")
endif()
