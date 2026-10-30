set(ng_expath_runtime_source_directories)
foreach(path_runtime_target IN ITEMS nicxlive nicxlive_shared)
    if(TARGET ${path_runtime_target})
        # The canonical SDK now defines the typed Path methods and allocation
        # Error bridge. Keep existing workspace include/feature visibility.
        target_include_directories(${path_runtime_target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(${path_runtime_target} PRIVATE cxx_std_20)
        get_target_property(ng_expath_runtime_source_directory ${path_runtime_target} SOURCE_DIR)
        list(APPEND ng_expath_runtime_source_directories "${ng_expath_runtime_source_directory}")
    endif()
endforeach()
list(REMOVE_DUPLICATES ng_expath_runtime_source_directories)
foreach(ng_expath_runtime_source_directory IN LISTS ng_expath_runtime_source_directories)
    # The selected canonical Source bodies keep finite guards and scalar order
    # even when the SDK's optional native fast-math policy is enabled.
    if(MSVC)
        set_property(SOURCE
            "${ng_expath_runtime_source_directory}/core/nodes/path_deformer.cpp"
            "${ng_expath_runtime_source_directory}/core/nodes/deformer/drivers/phys.cpp"
            DIRECTORY "${ng_expath_runtime_source_directory}"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${ng_expath_runtime_source_directory}/core/nodes/path_deformer.cpp"
            "${ng_expath_runtime_source_directory}/core/nodes/deformer/drivers/phys.cpp"
            DIRECTORY "${ng_expath_runtime_source_directory}"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endforeach()
if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_expath)
    add_library(nijigenerate_expath STATIC src/nijigenerate/ext/nodes/expathdeformer.cpp)
    target_include_directories(nijigenerate_expath PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_expath PUBLIC cxx_std_20)
    target_sources(nijigenerate_expath PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/expathdeformer.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/expathdeformer_node.hpp")
    target_link_libraries(nijigenerate_expath PUBLIC nijigenerate_runtime_core)
    # Public inherited depth helpers also preserve finite guards and scalar order.
    if(MSVC)
        target_compile_options(nijigenerate_expath PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_expath PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
