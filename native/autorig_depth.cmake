add_library(nijigenerate_autorig_depth STATIC src/nijigenerate/autorig/deterministic/depth.cpp)
target_compile_features(nijigenerate_autorig_depth PUBLIC cxx_std_20)
target_sources(nijigenerate_autorig_depth PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/depth.hpp")
target_link_libraries(nijigenerate_autorig_depth PUBLIC nijigenerate_core)
if(MSVC)
    target_compile_options(nijigenerate_autorig_depth PRIVATE /fp:strict)
else()
    target_compile_options(nijigenerate_autorig_depth PRIVATE -ffp-contract=off -fno-fast-math)
endif()
