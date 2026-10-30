if(TARGET nijigenerate_window_base AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_button AND TARGET nijigenerate_dummy
        AND TARGET nijigenerate_label AND TARGET nijigenerate_core
        AND NOT TARGET nijigenerate_paramsplit_window)
    add_library(nijigenerate_paramsplit_window STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/paramsplit.cpp")
    target_compile_features(nijigenerate_paramsplit_window PUBLIC cxx_std_20)
    target_sources(nijigenerate_paramsplit_window PUBLIC FILE_SET paramsplit_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/paramsplit.hpp")
    target_link_libraries(nijigenerate_paramsplit_window PUBLIC nijigenerate_window_base
        nijigenerate_runtime_core PRIVATE nijigenerate_button nijigenerate_dummy nijigenerate_label nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_paramsplit_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_paramsplit_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
