option(NIJIGENERATE_EMBED_RESOURCES "Compile the complete res directory as an embedded byte library" OFF)
option(NIJIGENERATE_ASSET_CATALOG "Expose application shaders and startup font/image resources" ON)

if(NIJIGENERATE_EMBED_RESOURCES)
    if(NOT NIJIGENERATE_PYTHON)
        message(FATAL_ERROR "NIJIGENERATE_EMBED_RESOURCES requires NIJIGENERATE_PYTHON")
    endif()
    set(resource_input_dir "${PROJECT_SOURCE_DIR}/res")
    set(resource_output_dir "${CMAKE_CURRENT_BINARY_DIR}/embedded-resources")
    file(GLOB_RECURSE resource_inputs CONFIGURE_DEPENDS LIST_DIRECTORIES false "${resource_input_dir}/*")
    list(SORT resource_inputs)
    string(JOIN "\n" resource_inventory ${resource_inputs})
    file(GENERATE OUTPUT "${resource_output_dir}/inputs.txt" CONTENT "${resource_inventory}\n")
    set(resource_sources "${resource_output_dir}/embedded.cpp")
    foreach(resource_input IN LISTS resource_inputs)
        file(RELATIVE_PATH resource_name "${resource_input_dir}" "${resource_input}")
        string(SHA256 resource_key "${resource_name}")
        list(APPEND resource_sources "${resource_output_dir}/resource_${resource_key}.cpp")
    endforeach()
    set(resource_header "${resource_output_dir}/include/nijigenerate/resources/embedded.hpp")
    set(resource_stamp "${resource_output_dir}/generated.stamp")
    add_custom_command(OUTPUT "${resource_stamp}"
        BYPRODUCTS ${resource_sources} "${resource_header}" "${resource_output_dir}/manifest.json"
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tools/embed_resources.py"
            --root "${resource_input_dir}" --output "${resource_output_dir}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${resource_stamp}"
        DEPENDS ${resource_inputs} "${resource_output_dir}/inputs.txt"
            "${CMAKE_CURRENT_LIST_DIR}/tools/embed_resources.py" "${CMAKE_CURRENT_LIST_FILE}"
        COMMENT "Embedding current resource bytes"
        VERBATIM)
    add_custom_target(nijigenerate_generate_resources DEPENDS "${resource_stamp}")
    set_source_files_properties(${resource_sources} "${resource_header}" PROPERTIES GENERATED TRUE)
    add_library(nijigenerate_resources STATIC ${resource_sources} "${resource_header}")
    add_dependencies(nijigenerate_resources nijigenerate_generate_resources)
    target_include_directories(nijigenerate_resources PUBLIC "${resource_output_dir}/include")
    target_link_libraries(nijigenerate_resources PUBLIC nijigenerate_native_options)
    if(NIJIGENERATE_ASSET_CATALOG)
        set(asset_inputs)
        foreach(asset_source IN ITEMS backend/gl.d viewport/depth/renderer.d windows/psddepthmap.d
                commands/depth/bone_gpu_async.d viewport/depth/draw/gpu.d atlas/atlas.d
                core/font.d core/window.d core/logo.d widgets/dialog.d windows/welcome.d)
            list(APPEND asset_inputs "${PROJECT_SOURCE_DIR}/source/nijigenerate/${asset_source}")
        endforeach()
        set(asset_source "${resource_output_dir}/assets.cpp")
        set(asset_header "${resource_output_dir}/include/nijigenerate/resources/assets.hpp")
        set(asset_stamp "${resource_output_dir}/assets.stamp")
        add_custom_command(OUTPUT "${asset_stamp}"
            BYPRODUCTS "${asset_source}" "${asset_header}" "${resource_output_dir}/asset-manifest.json"
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tools/generate_asset_catalog.py"
                --root "${PROJECT_SOURCE_DIR}" --output "${resource_output_dir}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${asset_stamp}"
            DEPENDS ${asset_inputs} ${resource_inputs} "${resource_output_dir}/inputs.txt"
                "${CMAKE_CURRENT_LIST_DIR}/tools/generate_asset_catalog.py"
                "${CMAKE_CURRENT_LIST_DIR}/tools/embed_resources.py" "${CMAKE_CURRENT_LIST_FILE}"
            VERBATIM)
        add_custom_target(nijigenerate_generate_asset_catalog DEPENDS "${asset_stamp}")
        set_source_files_properties("${asset_source}" "${asset_header}" PROPERTIES GENERATED TRUE)
        target_sources(nijigenerate_resources PRIVATE "${asset_source}" "${asset_header}")
        add_dependencies(nijigenerate_resources nijigenerate_generate_asset_catalog)
    endif()
    if(BUILD_TESTING)
        add_executable(nijigenerate_resources_probe "${CMAKE_CURRENT_LIST_DIR}/tests/resources_probe.cpp")
        target_link_libraries(nijigenerate_resources_probe PRIVATE nijigenerate_resources)
        add_test(NAME native_embedded_resources
            COMMAND nijigenerate_resources_probe "${resource_input_dir}")
        if(NIJIGENERATE_ASSET_CATALOG)
            add_executable(nijigenerate_asset_catalog_probe "${CMAKE_CURRENT_LIST_DIR}/tests/asset_catalog_probe.cpp")
            target_link_libraries(nijigenerate_asset_catalog_probe PRIVATE nijigenerate_resources)
            add_test(NAME native_asset_catalog
                COMMAND nijigenerate_asset_catalog_probe "${resource_input_dir}")
            if(NIJIGENERATE_D_COMPILER)
                add_test(NAME native_shader_source_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_shader_sources.py"
                        --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                        --native "$<TARGET_FILE:nijigenerate_asset_catalog_probe>"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/shader-source-evidence/$<CONFIG>")
            endif()
        endif()
        if(WIN32 AND MSVC)
            add_test(NAME native_resource_rebuild
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/check_resource_rebuild.py"
                    --cmake "${CMAKE_COMMAND}" --source-root "${PROJECT_SOURCE_DIR}"
                    --output "${PROJECT_SOURCE_DIR}/build/resource-rebuild")
        endif()
    endif()
endif()
