# Both .tmp staging and native canonical copies have the repository as parent.
get_filename_component(_mesh_declaration_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
target_sources(nijigenerate_core PRIVATE
    "${_mesh_declaration_root}/native/src/nijigenerate/core/math/mesh_declarations.hpp")
if(BUILD_TESTING AND NIJIGENERATE_RUNTIME_PROBE
        AND NOT TARGET nijigenerate_mesh_public_declarations)
    add_library(nijigenerate_mesh_public_declarations OBJECT
        "${_mesh_declaration_root}/native/tests/mesh_public_declarations.cpp")
    target_link_libraries(nijigenerate_mesh_public_declarations PRIVATE
        nijigenerate_core nicxlive::nicxlive)
endif()
