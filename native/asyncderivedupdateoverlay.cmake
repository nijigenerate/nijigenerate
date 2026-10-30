if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_input AND TARGET nijigenerate_window_theme
        AND NOT TARGET nijigenerate_asyncderivedupdateoverlay)
    add_library(nijigenerate_asyncderivedupdateoverlay STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/asyncderivedupdateoverlay.cpp")
    target_include_directories(nijigenerate_asyncderivedupdateoverlay PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_asyncderivedupdateoverlay PUBLIC cxx_std_20)
    target_sources(nijigenerate_asyncderivedupdateoverlay PUBLIC
        FILE_SET async_overlay_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/asyncderivedupdateoverlay.hpp")
    target_link_libraries(nijigenerate_asyncderivedupdateoverlay PUBLIC nijigenerate_runtime_core nijigenerate_input
        PRIVATE nijigenerate_window_theme)
    if(MSVC)
        target_compile_options(nijigenerate_asyncderivedupdateoverlay PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_asyncderivedupdateoverlay PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
