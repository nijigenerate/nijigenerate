if(TARGET nijigenerate_excamera AND TARGET nijigenerate_imageexport AND TARGET nijigenerate_dialog_queue
        AND TARGET nijigenerate_window_background
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_category
        AND TARGET nijigenerate_button AND TARGET nijigenerate_dummy AND TARGET nijigenerate_label
        AND NOT TARGET nijigenerate_imgexport_window)
    add_library(nijigenerate_imgexport_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/imgexport.cpp")
    target_compile_features(nijigenerate_imgexport_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_imgexport_window PUBLIC FILE_SET imgexport_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/imgexport.hpp")
    target_link_libraries(nijigenerate_imgexport_window PUBLIC nijigenerate_window_base nijigenerate_excamera
        PRIVATE nijigenerate_imageexport nijigenerate_dialog_queue nijigenerate_window_background
            nijigenerate_runtime_core nijigenerate_category nijigenerate_button nijigenerate_dummy nijigenerate_label)
    if(MSVC)
        target_compile_options(nijigenerate_imgexport_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_imgexport_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    # The existing root registers window_base after this fragment. CMake resolves
    # that real forward target reference at generation; validate it after all
    # production fragments have been processed, rather than omitting this body.
    function(ng_require_imgexport_window_providers)
        foreach(provider IN ITEMS nijigenerate_window_base nijigenerate_excamera
                nijigenerate_imageexport nijigenerate_dialog_queue nijigenerate_window_background
                nijigenerate_runtime_core nijigenerate_category nijigenerate_button
                nijigenerate_dummy nijigenerate_label)
            if(NOT TARGET "${provider}")
                message(FATAL_ERROR "Image export window requires actual provider ${provider}")
            endif()
        endforeach()
    endfunction()
    cmake_language(DEFER CALL ng_require_imgexport_window_providers)
endif()
