if(TARGET nijigenerate_autorig_registered AND TARGET nijigenerate_quadratic AND NOT TARGET nijigenerate_autorig_controls)
    include("${CMAKE_CURRENT_LIST_DIR}/autorig_regex.cmake")
    add_library(nijigenerate_autorig_controls STATIC
        src/nijigenerate/autorig/deterministic/controls.cpp
        src/nijigenerate/autorig/deterministic/controls_endpoint.cpp
        src/nijigenerate/autorig/deterministic/controls_frame.cpp
        src/nijigenerate/autorig/deterministic/controls_profile.cpp
        src/nijigenerate/autorig/deterministic/controls_blink.cpp
        src/nijigenerate/autorig/deterministic/controls_displacement.cpp
        src/nijigenerate/autorig/deterministic/controls_orientation.cpp
        src/nijigenerate/autorig/deterministic/controls_fixed_foot.cpp
        src/nijigenerate/autorig/deterministic/controls_skin.cpp
        src/nijigenerate/autorig/deterministic/controls_projection.cpp
        src/nijigenerate/autorig/deterministic/controls_cheek.cpp)
    target_include_directories(nijigenerate_autorig_controls PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_controls PUBLIC
        FILE_SET autorig_controls_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/controls.hpp")
    target_compile_features(nijigenerate_autorig_controls PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_controls PUBLIC nijigenerate_autorig_registered nijigenerate_quadratic)
    target_link_libraries(nijigenerate_autorig_controls PRIVATE nijigenerate_autorig_regex)
    if(MSVC)
        if(NOT TARGET Boost::multiprecision)
            find_package(Boost CONFIG REQUIRED COMPONENTS multiprecision)
        endif()
        target_link_libraries(nijigenerate_autorig_controls PRIVATE Boost::multiprecision)
        target_compile_options(nijigenerate_autorig_controls PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_controls PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
