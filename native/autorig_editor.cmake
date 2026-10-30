add_library(nijigenerate_autorig_editor STATIC
    src/nijigenerate/autorig/deterministic/editor.cpp)
target_sources(nijigenerate_autorig_editor PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES src/nijigenerate/autorig/deterministic/editor.hpp)
target_include_directories(nijigenerate_autorig_editor PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_features(nijigenerate_autorig_editor PUBLIC cxx_std_20)
target_link_libraries(nijigenerate_autorig_editor PUBLIC nijigenerate_autorig_framework nijigenerate_runtime_core)
if(MSVC)
    target_compile_options(nijigenerate_autorig_editor PRIVATE /fp:strict)
else()
    target_compile_options(nijigenerate_autorig_editor PRIVATE -ffp-contract=off -fno-fast-math)
endif()
