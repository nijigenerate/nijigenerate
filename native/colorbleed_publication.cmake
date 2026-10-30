# Publish the real overloads on their existing production source target.
function(ng_publish_colorbleed)
    if(NOT TARGET nijigenerate_runtime_core)
        message(FATAL_ERROR "Color bleed publication requires the actual runtime core producer")
    endif()
    get_target_property(runtime_sources nijigenerate_runtime_core SOURCES)
    set(body_selected FALSE)
    foreach(source IN LISTS runtime_sources)
        if(source MATCHES "(^|/)colorbleed[.]cpp$")
            set(body_selected TRUE)
        endif()
    endforeach()
    if(NOT body_selected)
        message(FATAL_ERROR "Color bleed publication requires the actual colorbleed.cpp body")
    endif()
    get_target_property(published nijigenerate_runtime_core NG_COLORBLEED_HEADER_PUBLISHED)
    if(NOT published)
        target_sources(nijigenerate_runtime_core PUBLIC FILE_SET colorbleed_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/core/colorbleed.hpp"
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/core/colorbleed_detail.hpp")
        set_property(TARGET nijigenerate_runtime_core PROPERTY NG_COLORBLEED_HEADER_PUBLISHED TRUE)
    endif()
endfunction()
