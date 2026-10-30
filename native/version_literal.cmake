if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_version_literal)
    add_library(nijigenerate_version_literal INTERFACE)
    target_compile_features(nijigenerate_version_literal INTERFACE cxx_std_17)
    target_include_directories(nijigenerate_version_literal INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_version_literal INTERFACE
        FILE_SET version_literal_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ver.hpp")
    target_link_libraries(nijigenerate_core PUBLIC nijigenerate_version_literal)
endif()
