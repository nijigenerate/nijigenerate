# The root module has public imports only; no translation unit or initializer.
target_sources(nijigenerate_core PUBLIC
    FILE_SET root_package_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/package.hpp")
