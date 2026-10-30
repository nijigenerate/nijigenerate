if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/set_deform_binding.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/set_deform_binding_complete.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/detail/parameter_keypoint.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE FILE_SET parameter_keypoint_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/detail/parameter_keypoint.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET model_binding_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/set_deform_binding.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/set_deform_binding_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/detail/model_command_map.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_model_editor_completion)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/detail/parameter_keypoint.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/model/detail/parameter_keypoint.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -fno-fast-math -ffp-contract=off)
    endif()
endif()
