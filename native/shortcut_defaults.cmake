if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/defaults.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET shortcut_defaults_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/defaults.hpp")
endif()
