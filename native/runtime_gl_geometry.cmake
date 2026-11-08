# Complete independent drawable/SoA/fence-policy/part/mask/blend production bodies.
if(TARGET nijigenerate_renderer)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_drawable_buffers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_part.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_blend.cpp")
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask_shaders.hpp")
    target_sources(nijigenerate_renderer PUBLIC
        FILE_SET runtime_gl_drawable_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_drawable_buffers.hpp")
    target_sources(nijigenerate_renderer PUBLIC
        FILE_SET runtime_gl_mask_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_renderer PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_renderer PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
