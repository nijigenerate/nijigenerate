if(TARGET nijigenerate_runtime_core)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nicxlive::common)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/default_random.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET default_random_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/default_random.hpp")
endif()
