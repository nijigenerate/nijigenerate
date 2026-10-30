if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_autorig_presentation)
    add_library(nijigenerate_autorig_presentation STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/presentation.cpp")
    target_include_directories(nijigenerate_autorig_presentation PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_presentation PUBLIC
        FILE_SET autorig_presentation_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/presentation.hpp")
    target_compile_features(nijigenerate_autorig_presentation PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_presentation PUBLIC nijigenerate_runtime_core
        PRIVATE nijigenerate_native_options)
endif()
