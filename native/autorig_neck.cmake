if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_autorig_neck)
    add_library(nijigenerate_autorig_neck STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/neck.cpp")
    target_include_directories(nijigenerate_autorig_neck PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_neck PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/neck.hpp")
    target_compile_features(nijigenerate_autorig_neck PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_neck PUBLIC nijigenerate_runtime_core
        PRIVATE nijigenerate_native_options)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/neck.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/neck.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
