if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base)
    add_library(nijigenerate_inpexport_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/inpexport.cpp")
    target_sources(nijigenerate_inpexport_window PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/inpexport.hpp")
    target_include_directories(nijigenerate_inpexport_window PRIVATE "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
    target_link_libraries(nijigenerate_inpexport_window PUBLIC nijigenerate_window_base
        nijigenerate_runtime_core PRIVATE nijigenerate_imagefmt_reader nijigenerate_button
        nijigenerate_dummy nijigenerate_label nijigenerate_tooltip nijigenerate_texture_widget
        nijigenerate_dialog_queue)
    if(MSVC)
        target_compile_options(nijigenerate_inpexport_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_inpexport_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
