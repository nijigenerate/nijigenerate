if(TARGET nijigenerate_tasks AND TARGET nijigenerate_osqp_bridge)
    add_library(nijigenerate_quadratic STATIC
        src/nijigenerate/autorig/solver/quadratic.cpp
        src/nijigenerate/autorig/solver/quadratic.hpp
        src/nijigenerate/autorig/solver/quadratic_types.hpp
        src/nijigenerate/autorig/solver/quadratic_sparse.hpp
        src/nijigenerate/autorig/solver/quadratic_sparse_sort.hpp
        src/nijigenerate/autorig/solver/osqp_abi.hpp)
    target_include_directories(nijigenerate_quadratic PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_quadratic PUBLIC
        FILE_SET quadratic_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/solver/quadratic.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/solver/quadratic_types.hpp")
    target_compile_features(nijigenerate_quadratic PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_quadratic PUBLIC nijigenerate_tasks nijigenerate_osqp_bridge)
    if(MSVC)
        target_compile_options(nijigenerate_quadratic PRIVATE /fp:strict)
        set_property(TARGET nijigenerate_quadratic PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    else()
        target_compile_options(nijigenerate_quadratic PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
