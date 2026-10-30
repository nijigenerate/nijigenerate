# The authored module has no declarations or runtime initialization.
target_sources(nijigenerate_core PUBLIC
    FILE_SET vertex_mesheditor_node_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/node.hpp")
