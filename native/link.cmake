add_library(nijigenerate_link STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/link.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/browser.cpp")
target_include_directories(nijigenerate_link PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(nijigenerate_link PUBLIC
    FILE_SET link_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/link.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/browser.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_temp_cstring.hpp")
target_link_libraries(nijigenerate_link PUBLIC nijigenerate_native_options)
if(WIN32)
    target_link_libraries(nijigenerate_link PRIVATE shell32)
endif()
