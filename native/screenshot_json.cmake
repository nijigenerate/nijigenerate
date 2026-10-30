# The complete view.d module shares the real editor/runtime providers.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/view.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/view_simple_commands.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/view_simple_commands.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET HEADERS FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp")
    target_link_libraries(nijigenerate_runtime_core PRIVATE nijigenerate_imagefmt_writer)
endif()
