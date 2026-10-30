# The data module has no runtime initializer or transport dependency.
if(NOT TARGET nijigenerate_acp_types)
    add_library(nijigenerate_acp_types INTERFACE)
    target_include_directories(nijigenerate_acp_types INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_acp_types INTERFACE cxx_std_20)
    if(TARGET nijigenerate_runtime_core)
        target_link_libraries(nijigenerate_acp_types INTERFACE nijigenerate_runtime_core)
    endif()
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_acp_types INTERFACE nijigenerate_native_options)
    endif()
    target_sources(nijigenerate_acp_types INTERFACE
        FILE_SET acp_types_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/types_status_level.hpp")
endif()
