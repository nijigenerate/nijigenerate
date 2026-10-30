if(TARGET nijigenerate_automesh_processor)
    target_sources(nijigenerate_automesh_processor PUBLIC
        FILE_SET automesh_skeleton_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/skeletonize.hpp")
endif()

if(TARGET nijigenerate_automesh_processor AND TARGET nijigenerate_automesh_common
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_tasks
        AND TARGET nijigenerate_category AND TARGET nijigenerate_drag AND TARGET nijigenerate_label)
    add_library(nijigenerate_automesh_skeleton STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/skeletonize.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/skeletonize_reflection.cpp")
    target_include_directories(nijigenerate_automesh_skeleton PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_skeleton PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_skeleton PUBLIC
        nijigenerate_automesh_processor nijigenerate_automesh_common
        nijigenerate_runtime_core nijigenerate_tasks nijigenerate_core
        nijigenerate_category nijigenerate_drag nijigenerate_label)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_skeleton PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_skeleton PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
