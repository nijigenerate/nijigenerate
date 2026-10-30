if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/base.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET mesh_editor_operations_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/base.hpp")
    if(MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/base.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    endif()
endif()
