# Include after runtime, extended parameter/puppet, project and binding action registrations.
if(TARGET nicxlive::nicxlive AND TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/parameter.cpp")
    target_link_libraries(nijigenerate_core PUBLIC nicxlive::nicxlive)
    if(NOT TARGET nijigenerate_parameter_actions)
        # The canonical parameter.hpp templates share the real parameter.cpp helpers.
        add_library(nijigenerate_parameter_actions INTERFACE)
        target_include_directories(nijigenerate_parameter_actions INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_sources(nijigenerate_parameter_actions INTERFACE FILE_SET parameter_action_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/parameter.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/parameter_arguments.hpp")
        target_compile_features(nijigenerate_parameter_actions INTERFACE cxx_std_20)
        # The actual NullableString formatter and project/extended-parameter
        # providers are compiled into runtime_core, including actions/node.cpp.
        target_link_libraries(nijigenerate_parameter_actions INTERFACE
            nijigenerate_core nijigenerate_runtime_core)
    endif()
endif()
