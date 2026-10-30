# Complete independent drawable/SoA/fence-policy/part/mask/blend production bodies.
if(TARGET nijigenerate_gl_backend_source)
    target_sources(nijigenerate_gl_backend_source PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_drawable_buffers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_part.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_blend.cpp")
    target_sources(nijigenerate_gl_backend_source PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask_shaders.hpp")
    target_sources(nijigenerate_gl_backend_source PUBLIC
        FILE_SET runtime_gl_drawable_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_drawable_buffers.hpp")
    target_sources(nijigenerate_gl_backend_source PUBLIC
        FILE_SET runtime_gl_mask_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_mask.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_gl_backend_source PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_gl_backend_source PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
