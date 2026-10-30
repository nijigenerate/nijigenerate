if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/base_internal.hpp")
    target_sources(nijigenerate_core PUBLIC
        FILE_SET shortcut_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/base.hpp")
endif()
