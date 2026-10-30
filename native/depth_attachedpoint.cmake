if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/attachedpoint.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/attachedpoint_interaction.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_attachedpoint_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/attachedpoint.hpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/attachedpoint_interaction.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/attachedpoint_interaction.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
