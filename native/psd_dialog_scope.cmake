if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_scope.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET psd_dialog_scope_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/psd_dialog_scope.hpp")
endif()
