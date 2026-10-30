if(TARGET nijigenerate_automesh_alpha_provider AND TARGET nijigenerate_viewport_mesh)
    add_library(nijigenerate_automesh_common STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/common.cpp")
    target_include_directories(nijigenerate_automesh_common PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_automesh_common PUBLIC
        FILE_SET automesh_common_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/common.hpp")
    target_compile_features(nijigenerate_automesh_common PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_common PUBLIC
        nijigenerate_automesh_alpha_provider nijigenerate_viewport_mesh)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_common PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_common PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
