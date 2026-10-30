if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base AND TARGET nijigenerate_inputtext)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_list.cpp")
    add_library(nijigenerate_editanim_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/editanim.cpp")
    target_sources(nijigenerate_editanim_window PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/editanim.hpp")
    target_link_libraries(nijigenerate_editanim_window PUBLIC nijigenerate_window_base
        nijigenerate_inputtext nijigenerate_runtime_core PRIVATE nijigenerate_button
        nijigenerate_dummy nijigenerate_label nijigenerate_category nijigenerate_dialog_queue)
    if(MSVC)
        target_compile_options(nijigenerate_editanim_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_editanim_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
