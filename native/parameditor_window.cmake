if(TARGET nijigenerate_window_base AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_controller AND TARGET nijigenerate_inputtext
        AND TARGET nijigenerate_drag AND TARGET nijigenerate_button
        AND TARGET nijigenerate_dummy AND TARGET nijigenerate_label
        AND TARGET nijigenerate_dialogs AND NOT TARGET nijigenerate_parameditor_window)
    add_library(nijigenerate_parameditor_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/parameditor.cpp")
    target_compile_features(nijigenerate_parameditor_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_parameditor_window PUBLIC FILE_SET parameditor_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/parameditor.hpp")
    target_link_libraries(nijigenerate_parameditor_window PUBLIC nijigenerate_window_base
        nijigenerate_runtime_core nijigenerate_controller nijigenerate_inputtext PRIVATE
        nijigenerate_drag nijigenerate_button nijigenerate_dummy nijigenerate_label nijigenerate_dialogs)
    if(MSVC)
        target_compile_options(nijigenerate_parameditor_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_parameditor_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
