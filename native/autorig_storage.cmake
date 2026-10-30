if(TARGET nijigenerate_autorig_framework)
    add_library(nijigenerate_autorig_storage STATIC src/nijigenerate/autorig/deterministic/storage.cpp)
    target_compile_features(nijigenerate_autorig_storage PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_storage PUBLIC FILE_SET storage_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/storage.hpp")
    target_link_libraries(nijigenerate_autorig_storage PUBLIC
        nijigenerate_autorig_framework nijigenerate_autorig_contracts)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_storage PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_storage PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
