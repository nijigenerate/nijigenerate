if(NOT TARGET nijigenerate_physics_geometry)
    if(NOT TARGET Boost::json)
        find_package(Boost 1.90 CONFIG REQUIRED COMPONENTS json)
    endif()
    add_library(nijigenerate_physics_geometry STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/physics.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/physics_math_detail.hpp")
    target_include_directories(nijigenerate_physics_geometry PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_physics_geometry PUBLIC cxx_std_20)
    target_sources(nijigenerate_physics_geometry PUBLIC
        FILE_SET physics_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/physics.hpp")
    target_link_libraries(nijigenerate_physics_geometry PUBLIC nijigenerate_native_options Boost::json)
    if(MSVC)
        target_compile_options(nijigenerate_physics_geometry PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_physics_geometry PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
