if(TARGET nijigenerate_automesh_processor)
    target_sources(nijigenerate_automesh_processor PUBLIC
        FILE_SET automesh_grid_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/grid.hpp")
endif()

if(TARGET nijigenerate_automesh_processor AND TARGET nijigenerate_automesh_common
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_category
        AND TARGET nijigenerate_drag AND TARGET nijigenerate_label
        AND TARGET nijigenerate_dummy AND TARGET nijigenerate_button)
    add_library(nijigenerate_automesh_grid STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/grid.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/grid_legacy_json.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/grid_reflection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/grid_legacy_ui.cpp")
    target_include_directories(nijigenerate_automesh_grid PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_grid PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_grid PUBLIC
        nijigenerate_automesh_processor nijigenerate_automesh_common nijigenerate_runtime_core nijigenerate_core
        nijigenerate_category nijigenerate_drag nijigenerate_label nijigenerate_dummy nijigenerate_button)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_grid PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_grid PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
