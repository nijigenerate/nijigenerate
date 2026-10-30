if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_spline AND
        TARGET nijigenerate_viewport_base_primitives AND TARGET nijigenerate_viewport_common AND
        NOT TARGET nijigenerate_model_drawable_editor)
    add_library(nijigenerate_model_drawable_editor STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/drawable.cpp")
    target_include_directories(nijigenerate_model_drawable_editor PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_model_drawable_editor PUBLIC cxx_std_20)
    target_sources(nijigenerate_model_drawable_editor PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/drawable.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/drawable.tpp")
    target_link_libraries(nijigenerate_model_drawable_editor PUBLIC nijigenerate_runtime_core
        nijigenerate_spline nijigenerate_debug nijigenerate_viewport_base_primitives nijigenerate_viewport_common)
    if(MSVC)
        target_compile_options(nijigenerate_model_drawable_editor PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_model_drawable_editor PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
