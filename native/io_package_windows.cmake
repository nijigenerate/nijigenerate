# Preserve the established root entrypoint while selecting the original OS branch.
if(APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/io_package_macos.cmake")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/io_package_linux.cmake")
endif()

if(WIN32 AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_core AND TARGET nijigenerate_dialog_queue
        AND NOT TARGET nijigenerate_io)
    if(NOT TARGET nijigenerate_tinyfiledialogs)
        message(FATAL_ERROR "io_package_windows.cmake requires tinyfiledialogs_windows.cmake")
    endif()
    add_library(nijigenerate_io STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package_windows.cpp")
    target_include_directories(nijigenerate_io PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_io PUBLIC nijigenerate_runtime_core nijigenerate_core
        nijigenerate_dialog_queue nijigenerate_tinyfiledialogs)
    target_compile_features(nijigenerate_io PUBLIC cxx_std_20)
    target_sources(nijigenerate_io PUBLIC FILE_SET io_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package.hpp")
endif()
