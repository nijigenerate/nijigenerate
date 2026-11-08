# Complete rendering implementations of the independent texture, handles,
# draw_texture, dynamic_composite and diff_collect_impl source modules.
if(TARGET nijigenerate_renderer)
    target_link_libraries(nijigenerate_renderer PUBLIC nicxlive::nicxlive)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_texture.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_draw_texture.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_dynamic_composite.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_diff_collect.cpp")
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_diff_collect_assets.hpp")
    target_sources(nijigenerate_renderer PUBLIC
        FILE_SET runtime_gl_diff_collect_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_diff_collect.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_renderer PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_renderer PRIVATE -ffp-contract=off)
    endif()
endif()
