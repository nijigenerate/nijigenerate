if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive
        AND NOT TARGET nijigenerate_ext_parameter)
    # The actual param.cpp is already part of runtime_core.
    add_library(nijigenerate_ext_parameter INTERFACE)
    target_include_directories(nijigenerate_ext_parameter INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_ext_parameter INTERFACE cxx_std_20)
    target_sources(nijigenerate_ext_parameter INTERFACE FILE_SET ext_parameter_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/param.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/param_metadata.hpp")
    target_link_libraries(nijigenerate_ext_parameter INTERFACE nijigenerate_runtime_core)
endif()
