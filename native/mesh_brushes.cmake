if(TARGET nijigenerate_viewport_common AND TARGET nijigenerate_debug AND TARGET nijigenerate_drag)
    add_library(nijigenerate_mesh_brushes STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/circlebrush.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/doublethreshbrush.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/rectanglebrush.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/package.cpp")
    target_include_directories(nijigenerate_mesh_brushes PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_mesh_brushes PUBLIC
        FILE_SET mesh_brush_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/package.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/base.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/circlebrush.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/doublethreshbrush.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/rectanglebrush.hpp")
    target_compile_features(nijigenerate_mesh_brushes PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_mesh_brushes PUBLIC
        nijigenerate_viewport_common nijigenerate_debug nijigenerate_drag)
    if(MSVC)
        target_compile_options(nijigenerate_mesh_brushes PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_mesh_brushes PRIVATE -ffp-contract=off)
    endif()
endif()
