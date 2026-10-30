if(TARGET nijigenerate_window_base AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_inputtext AND TARGET nijigenerate_button
        AND TARGET nijigenerate_dummy AND TARGET nijigenerate_label
        AND TARGET nijigenerate_tooltip AND TARGET nijigenerate_core
        AND TARGET nijigenerate_expart
        AND NOT TARGET nijigenerate_psdmerge_window)
    add_library(nijigenerate_psdmerge_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psdmerge.cpp")
    target_compile_features(nijigenerate_psdmerge_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_psdmerge_window PUBLIC FILE_SET psdmerge_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psdmerge.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psdmerge_path.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psdmerge_depth_sort.hpp")
    target_link_libraries(nijigenerate_psdmerge_window PUBLIC nijigenerate_window_base
        nijigenerate_runtime_core PRIVATE nijigenerate_inputtext nijigenerate_button
        nijigenerate_dummy nijigenerate_label nijigenerate_tooltip nijigenerate_core
        nijigenerate_expart)
    if(MSVC)
        target_compile_options(nijigenerate_psdmerge_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_psdmerge_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
