if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_expart
        AND TARGET nijigenerate_excamera AND TARGET nijigenerate_depthbone_nodes
        AND TARGET nijigenerate_exgrid AND TARGET nijigenerate_expath
        AND TARGET nijigenerate_ext_parameter
        AND NOT TARGET nijigenerate_ext_initialization)
    add_library(nijigenerate_ext_initialization STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/initialization.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/initialization.cpp")
    target_include_directories(nijigenerate_ext_initialization PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_ext_initialization PUBLIC cxx_std_20)
    target_sources(nijigenerate_ext_initialization PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/package.hpp")
    target_link_libraries(nijigenerate_ext_initialization PUBLIC
        nijigenerate_runtime_core nijigenerate_expart nijigenerate_excamera
        nijigenerate_depthbone_nodes nijigenerate_exgrid nijigenerate_expath
        nijigenerate_ext_parameter)
    # The actual application calls incInitExt and consumes runtime_core. Keep the
    # real initializer/provider archive on that ordinary link interface; this
    # does not run initialization during import or synthesize an executable.
    # Both runtime and initializer are STATIC and already share node providers.
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_ext_initialization)
endif()
