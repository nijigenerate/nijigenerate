# Genuine standard-library/name source closure; validation is project-wide.
target_sources(nijigenerate_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_names.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_composite_aliases.cpp")

# Canonical private records use the actual runtime math/array/node providers.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_private_values.cpp")
endif()
