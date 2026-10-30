if(TARGET nijigenerate_window_base AND TARGET nijigenerate_inputtext
        AND TARGET nijigenerate_button AND TARGET nijigenerate_dummy
        AND TARGET nijigenerate_core AND NOT TARGET nijigenerate_rename_window)
    add_library(nijigenerate_rename_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/rename.cpp")
    target_link_libraries(nijigenerate_rename_window PUBLIC nijigenerate_window_base
        nijigenerate_inputtext PRIVATE nijigenerate_button nijigenerate_dummy nijigenerate_core)
    target_compile_features(nijigenerate_rename_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_rename_window PUBLIC FILE_SET rename_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/rename.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_rename_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_rename_window PRIVATE -ffp-contract=off)
    endif()
endif()
