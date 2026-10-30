if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET composite_inspector_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/composite.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/composite_run.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/dragdrop_mask_payload.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/detail/source_c_string_contract.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/assert_error.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_blend_mode.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/dragdrop_mask_payload.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/composite_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common_deform.cpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/composite_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/composite_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
