if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_native_options
        AND NOT TARGET nijigenerate_autorig_review)
    add_library(nijigenerate_autorig_review STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/review.cpp")
    target_include_directories(nijigenerate_autorig_review PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_autorig_review PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_review PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/review.hpp")
    target_link_libraries(nijigenerate_autorig_review PUBLIC nijigenerate_runtime_core
        PRIVATE nijigenerate_native_options)
endif()
