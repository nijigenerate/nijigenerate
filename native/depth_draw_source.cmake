# Include after Session and actual PSD/PNG/image providers are registered.
if(TARGET nijigenerate_runtime_core)
    set(depth_source "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/source.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE "${depth_source}")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET depth_draw_source_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/source.hpp")
    target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
    if(MSVC)
        set_source_files_properties("${depth_source}" PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        set_source_files_properties("${depth_source}" PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
