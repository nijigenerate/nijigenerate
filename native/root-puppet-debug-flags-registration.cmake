# Register the complete M092 source file and its actual public declarations.
target_sources(nijigenerate_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/base.cpp")
target_sources(nijigenerate_core PUBLIC FILE_SET puppet_debug_flags_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/base.hpp")
if(BUILD_TESTING AND NOT TARGET nijigenerate_puppet_debug_flags_probe)
    add_executable(nijigenerate_puppet_debug_flags_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/puppet_debug_flags_probe.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/puppet_debug_flags_addresses.cpp")
    target_link_libraries(nijigenerate_puppet_debug_flags_probe PRIVATE nijigenerate_core)
    add_test(NAME native_puppet_debug_flags_file COMMAND nijigenerate_puppet_debug_flags_probe)
endif()
