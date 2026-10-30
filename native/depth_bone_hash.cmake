include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone hashes after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_hash_primitives.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_structure_hashes.cpp")
if(MSVC)
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_hash_primitives.cpp"
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_hash_primitives.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math -frounding-math)
endif()
