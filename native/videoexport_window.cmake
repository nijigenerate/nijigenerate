if((WIN32 OR UNIX) AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_excamera
        AND TARGET nijigenerate_window_base AND TARGET nijigenerate_dialog_queue
        AND TARGET nijigenerate_category AND TARGET nijigenerate_button
        AND TARGET nijigenerate_label AND TARGET nijigenerate_tooltip
        AND NOT TARGET nijigenerate_videoexport_window)
    add_library(nijigenerate_videoexport_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/videoexport.cpp")
    target_compile_features(nijigenerate_videoexport_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_videoexport_window PUBLIC FILE_SET videoexport_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/videoexport.hpp")
    target_link_libraries(nijigenerate_videoexport_window PUBLIC nijigenerate_window_base nijigenerate_excamera
        nijigenerate_runtime_core PRIVATE nijigenerate_dialog_queue nijigenerate_category
            nijigenerate_button nijigenerate_label nijigenerate_tooltip)
    if(MSVC)
        target_compile_options(nijigenerate_videoexport_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_videoexport_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
