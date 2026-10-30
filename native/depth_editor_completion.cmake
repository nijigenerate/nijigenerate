# Source-level depth editor/operation closure. Existing CPU bodies are compiled
# into production once; their separate regression object targets remain intact.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_gl_backend_source AND
        TARGET nijigenerate_debug AND TARGET nijigenerate_viewport_base_primitives)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/renderer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/renderer_gl.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node_render.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/editor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/action.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_value_labels.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_render.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/editor.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_sort.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/action.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/editor.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node_stable_sort.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/detail/editor_log_format.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/renderer.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/detail/float_sort.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET depth_editor_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/editor.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/editor_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/editor_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/editor_table.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_gl_backend_source
        nijigenerate_debug nijigenerate_viewport_base_primitives nijigenerate_input
        nijigenerate_drag nijigenerate_core OpenGL::GL)
    if(TARGET nijigenerate_input AND TARGET nijigenerate_drag AND TARGET OpenGL::GL
            AND NOT TARGET nijigenerate_depth_editor_commands)
        add_library(nijigenerate_depth_editor_commands INTERFACE)
        target_include_directories(nijigenerate_depth_editor_commands INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_depth_editor_commands INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_depth_editor_commands INTERFACE nijigenerate_runtime_core)
    endif()
    if(TARGET nijigenerate_input AND TARGET nijigenerate_drag AND TARGET OpenGL::GL
            AND NOT TARGET nijigenerate_depth_renderer)
        add_library(nijigenerate_depth_renderer INTERFACE)
        target_include_directories(nijigenerate_depth_renderer INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_depth_renderer INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_depth_renderer INTERFACE nijigenerate_runtime_core
            nijigenerate_gl_backend_source OpenGL::GL)
    endif()
    if(TARGET nijigenerate_input AND TARGET nijigenerate_drag AND TARGET OpenGL::GL
            AND TARGET nijigenerate_imgui)
        foreach(depth_editor_module IN ITEMS nijigenerate_depth_mesh_editor nijigenerate_depth_mesh_node)
            if(NOT TARGET ${depth_editor_module})
                add_library(${depth_editor_module} INTERFACE)
                target_include_directories(${depth_editor_module} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
                target_compile_features(${depth_editor_module} INTERFACE cxx_std_20)
                target_link_libraries(${depth_editor_module} INTERFACE nijigenerate_runtime_core nijigenerate_imgui)
            endif()
        endforeach()
        if(NOT TARGET nijigenerate_depth_operations)
            add_library(nijigenerate_depth_operations INTERFACE)
            target_include_directories(nijigenerate_depth_operations INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
            target_compile_features(nijigenerate_depth_operations INTERFACE cxx_std_20)
            target_link_libraries(nijigenerate_depth_operations INTERFACE nijigenerate_runtime_core nijigenerate_imgui)
        endif()
    endif()
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/renderer_gl.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node_render.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/editor.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_labels.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_value_labels.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_render.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/editor.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_labels.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_value_labels.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/operation_render.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
