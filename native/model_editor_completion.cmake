if(TARGET nijigenerate_mesh_editor_aggregate AND TARGET nijigenerate_model_drawable_editor)
    add_library(nijigenerate_model_editor_completion STATIC
        src/nijigenerate/viewport/model/mesheditor/deformable.cpp
        src/nijigenerate/viewport/model/mesheditor/editor.cpp
        src/nijigenerate/viewport/model/deform.cpp
        src/nijigenerate/viewport/model/package.cpp
        src/nijigenerate/viewport/model/depthboneoverlay.cpp)
    target_include_directories(nijigenerate_model_editor_completion PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_model_editor_completion PUBLIC cxx_std_20)
    target_sources(nijigenerate_model_editor_completion PUBLIC
        FILE_SET model_editor_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/deformable.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/mesheditor/editor.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/model/deform.hpp")
    target_link_libraries(nijigenerate_model_editor_completion PUBLIC
        nijigenerate_mesh_editor_aggregate nijigenerate_model_drawable_editor
        nijigenerate_viewport_base_primitives nijigenerate_debug)
    if(MSVC)
        target_compile_options(nijigenerate_model_editor_completion PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_model_editor_completion PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/depth_bone_overlay_publication.cmake")
    ng_publish_depth_bone_overlay()
endif()
