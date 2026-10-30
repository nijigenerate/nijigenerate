if(TARGET nijigenerate_crashdump)
    return()
endif()
if(NOT TARGET nijigenerate_runtime_core OR NOT TARGET nijigenerate_core)
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/crashdump_common.cmake")
if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_complete.cmake")
elseif(UNIX AND NOT TARGET nijigenerate_crashdump_posix)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump_posix.cmake")
endif()

# The public module only connects existing providers. Handler installation stays
# an explicit application/worker call; no module initializer is introduced.
add_library(nijigenerate_crashdump INTERFACE)
target_include_directories(nijigenerate_crashdump INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_features(nijigenerate_crashdump INTERFACE cxx_std_20)
target_sources(nijigenerate_crashdump INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_text.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_report.hpp")
target_link_libraries(nijigenerate_crashdump INTERFACE
    nijigenerate_crashdump_common nijigenerate_runtime_core nijigenerate_core)
if(WIN32)
    # The public module includes this platform declaration header directly.
    target_sources(nijigenerate_crashdump INTERFACE FILE_SET HEADERS FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_windows_state.hpp")
    target_link_libraries(nijigenerate_crashdump INTERFACE nijigenerate_crashdump_windows)
elseif(UNIX)
    target_sources(nijigenerate_crashdump INTERFACE FILE_SET HEADERS FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_state.hpp")
    target_link_libraries(nijigenerate_crashdump INTERFACE
        nijigenerate_crashdump_posix nijigenerate_crashdump_posix_state nijigenerate_crashdump_posix_io)
endif()
