# Include after depth_draw_session.cmake.
if(TARGET nijigenerate_runtime_core)
    set(depth_diagnostics_source "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/diagnostics.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE "${depth_diagnostics_source}")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/diagnostics_sort.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET depth_draw_diagnostics_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/diagnostics.hpp")
    if(MSVC)
        set_source_files_properties("${depth_diagnostics_source}" PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        set_source_files_properties("${depth_diagnostics_source}" PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
