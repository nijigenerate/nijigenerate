if(TARGET nijigenerate_excamera AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_depthbone_nodes AND NOT TARGET nijigenerate_transformhandle)
    add_library(nijigenerate_transformhandle STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/transformhandle.cpp")
    target_include_directories(nijigenerate_transformhandle PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_transformhandle PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/transformhandle.hpp")
    target_compile_features(nijigenerate_transformhandle PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_transformhandle PUBLIC nijigenerate_runtime_core
        nijigenerate_excamera nijigenerate_depthbone_nodes)
    if(MSVC)
        target_compile_options(nijigenerate_transformhandle PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_transformhandle PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
