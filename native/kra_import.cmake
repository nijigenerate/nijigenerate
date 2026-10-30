# M154 source bodies. The generic M152 importer remains a required actual provider.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET kra_adapter_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/kra.hpp")
    target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
endif()
