# The original facade only publicly imports the three genuine producer modules.
target_sources(nijigenerate_core PUBLIC
    FILE_SET mesh_editor_operations_package_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/package.hpp")
