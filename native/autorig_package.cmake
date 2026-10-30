if(TARGET nijigenerate_autorig_framework AND TARGET nijigenerate_autorig_workflow
        AND NOT TARGET nijigenerate_autorig)
    add_library(nijigenerate_autorig INTERFACE)
    target_sources(nijigenerate_autorig INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/package.hpp")
    target_include_directories(nijigenerate_autorig INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_autorig INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_autorig INTERFACE
        nijigenerate_autorig_framework nijigenerate_autorig_workflow)
endif()
