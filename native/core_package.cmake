# Publish the authored core facade on the existing actual mainFont module target.
if(TARGET nijigenerate_core_package)
    target_sources(nijigenerate_core_package PUBLIC
        FILE_SET core_module_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core.hpp")
endif()

# The atlas packer is compiled by the same normal numeric core target.
if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PUBLIC
        FILE_SET atlas_packer_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/packer.hpp")
endif()

# Config has no ImGui dependency or runtime initialization.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET config_module_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/config.hpp")
endif()

# The registry's public snapshots use the existing actual MonoTime carrier.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET async_derived_update_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/asyncderivedupdate.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_monotime.hpp")
endif()
