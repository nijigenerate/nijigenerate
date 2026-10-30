# Reuse the actual overlay and cache bodies after product provider selection.
function(ng_publish_depth_bone_overlay)
    foreach(provider IN ITEMS nijigenerate_model_editor_completion nijigenerate_runtime_core nijigenerate_debug)
        if(NOT TARGET "${provider}")
            message(FATAL_ERROR "Depth bone overlay requires actual provider ${provider}")
        endif()
    endforeach()
    # Select the existing real cache/runtime source group under its own unchanged
    # product selection. Its include guards preserve earlier individual imports.
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/depth_bone_port.cmake")
    get_target_property(runtime_sources nijigenerate_runtime_core SOURCES)
    set(cache_source_selected FALSE)
    foreach(source IN LISTS runtime_sources)
        if(source MATCHES "(^|/)bone_effective_pivots[.]cpp$")
            set(cache_source_selected TRUE)
        endif()
    endforeach()
    if(NOT cache_source_selected)
        message(FATAL_ERROR "Depth bone overlay requires the actual bone_effective_pivots.cpp producer")
    endif()
    get_target_property(published nijigenerate_model_editor_completion NG_DEPTH_BONE_OVERLAY_PUBLISHED)
    if(NOT published)
        target_sources(nijigenerate_model_editor_completion PUBLIC FILE_SET depth_bone_overlay_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/viewport/model/depthboneoverlay.hpp")
        target_link_libraries(nijigenerate_model_editor_completion PUBLIC
            nijigenerate_runtime_core nijigenerate_debug)
        set_property(TARGET nijigenerate_model_editor_completion PROPERTY NG_DEPTH_BONE_OVERLAY_PUBLISHED TRUE)
    endif()
endfunction()
