if(TARGET nijigenerate_core AND TARGET nijigenerate_runtime_core
    AND TARGET nijigenerate_autorig_framework AND NOT TARGET nijigenerate_autorig_contracts)
    add_library(nijigenerate_autorig_contracts INTERFACE)
    target_sources(nijigenerate_autorig_contracts INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/contracts.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/contracts_json.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/contracts_file.hpp")
    target_include_directories(nijigenerate_autorig_contracts INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_autorig_contracts INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_autorig_contracts INTERFACE
        nijigenerate_core nijigenerate_runtime_core nijigenerate_autorig_framework)
endif()
