if(TARGET nijigenerate_autorig_evidence AND TARGET nijigenerate_physics_geometry AND NOT TARGET nijigenerate_autorig_physics)
    include("${CMAKE_CURRENT_LIST_DIR}/autorig_regex.cmake")
    add_library(nijigenerate_autorig_physics STATIC
        src/nijigenerate/autorig/deterministic/physics_policy.cpp
        src/nijigenerate/autorig/deterministic/physics_weights.cpp
        src/nijigenerate/autorig/deterministic/physics_asset.cpp
        src/nijigenerate/autorig/deterministic/physics_structure.cpp
        src/nijigenerate/autorig/deterministic/physics_operations.cpp)
    target_compile_features(nijigenerate_autorig_physics PUBLIC cxx_std_20)
    set(ng_physics_resource_root "${PROJECT_SOURCE_DIR}/res/autorig")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${ng_physics_resource_root}/physics-policy.json"
        "${ng_physics_resource_root}/physics-material-roles.json")
    foreach(ng_physics_resource IN ITEMS policy material-roles)
        file(READ "${ng_physics_resource_root}/physics-${ng_physics_resource}.json"
            ng_physics_resource_hex HEX)
        string(LENGTH "${ng_physics_resource_hex}" ng_physics_resource_hex_length)
        math(EXPR ng_physics_resource_size "${ng_physics_resource_hex_length} / 2")
        string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "static_cast<char>(0x\\1),"
            ng_physics_resource_bytes "${ng_physics_resource_hex}")
        if(ng_physics_resource STREQUAL "policy")
            set(ng_physics_policy_bytes "${ng_physics_resource_bytes}")
            set(ng_physics_policy_size "${ng_physics_resource_size}")
        else()
            set(ng_physics_roles_bytes "${ng_physics_resource_bytes}")
            set(ng_physics_roles_size "${ng_physics_resource_size}")
        endif()
    endforeach()
    set(ng_physics_generated "${CMAKE_CURRENT_BINARY_DIR}/physics-generated")
    file(GENERATE OUTPUT
        "${ng_physics_generated}/nijigenerate/autorig/deterministic/detail/physics_resources_generated.hpp"
        CONTENT
"#pragma once
#include <cstddef>
#include <string_view>
namespace nijigenerate::autorig::deterministic::physics::detail {
inline constexpr char policyBytes[] = {${ng_physics_policy_bytes}char{}};
inline constexpr char rolesBytes[] = {${ng_physics_roles_bytes}char{}};
inline std::string_view policySource() { return {policyBytes, ${ng_physics_policy_size}}; }
inline std::string_view materialRolesSource() { return {rolesBytes, ${ng_physics_roles_size}}; }
}
")
    target_include_directories(nijigenerate_autorig_physics PRIVATE "${ng_physics_generated}")
    target_link_libraries(nijigenerate_autorig_physics PUBLIC nijigenerate_autorig_evidence nijigenerate_physics_geometry)
    target_link_libraries(nijigenerate_autorig_physics PRIVATE nijigenerate_autorig_regex)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_physics PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_physics PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
