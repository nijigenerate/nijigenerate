if(TARGET nijigenerate_tasks AND TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_autorig_framework STATIC
        src/nijigenerate/autorig/framework_values.cpp
        src/nijigenerate/autorig/framework_context.cpp
        src/nijigenerate/autorig/framework_session_run.cpp
        src/nijigenerate/autorig/framework_session_state.cpp
        src/nijigenerate/autorig/framework_manager.cpp)
    target_sources(nijigenerate_autorig_framework PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/framework.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/framework_schema.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/framework_primitives.hpp")
    target_sources(nijigenerate_autorig_framework PUBLIC FILE_SET source_temp_directory_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_temp_directory.hpp")
    target_compile_features(nijigenerate_autorig_framework PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_framework PUBLIC nijigenerate_tasks nijigenerate_runtime_core)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_framework PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_framework PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
