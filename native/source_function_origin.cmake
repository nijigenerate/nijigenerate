if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PUBLIC
        FILE_SET source_function_origin_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_function_origin.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_static_function_origin.hpp")
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_function_origin.cpp")
endif()
