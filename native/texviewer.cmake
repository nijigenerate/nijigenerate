if(TARGET nijigenerate_window_base AND TARGET nicxlive::nicxlive
        AND TARGET nijigenerate_core AND NOT TARGET nijigenerate_texviewer)
    add_library(nijigenerate_texviewer STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/texviewer.cpp")
    target_link_libraries(nijigenerate_texviewer PUBLIC nijigenerate_window_base nicxlive::nicxlive
        PRIVATE nijigenerate_core)
    target_compile_features(nijigenerate_texviewer PUBLIC cxx_std_20)
    target_sources(nijigenerate_texviewer PUBLIC FILE_SET texviewer_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/texviewer.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_texviewer PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_texviewer PRIVATE -ffp-contract=off)
    endif()
endif()
