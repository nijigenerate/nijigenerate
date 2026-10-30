if(TARGET nijigenerate_autorig_framework)
    add_library(nijigenerate_autorig_pipeline STATIC src/nijigenerate/autorig/deterministic/pipeline.cpp)
    target_include_directories(nijigenerate_autorig_pipeline PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_pipeline PUBLIC
        FILE_SET autorig_pipeline_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/pipeline.hpp")
    target_compile_features(nijigenerate_autorig_pipeline PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_pipeline PUBLIC nijigenerate_autorig_framework)
endif()
