if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_inputtext AND TARGET nijigenerate_window_base)
    add_library(nijigenerate_flipconfig STATIC
        src/nijigenerate/windows/flipconfig.cpp src/nijigenerate/windows/flipconfig_window.cpp)
    target_compile_features(nijigenerate_flipconfig PUBLIC cxx_std_20)
    target_sources(nijigenerate_flipconfig PUBLIC
        FILE_SET flip_pair_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/flipconfig.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/flipconfig_model.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/flipconfig_window.hpp")
    target_link_libraries(nijigenerate_flipconfig PUBLIC nijigenerate_runtime_core
        nijigenerate_inputtext nijigenerate_window_base
        PRIVATE nijigenerate_button nijigenerate_label nijigenerate_dummy nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_flipconfig PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_flipconfig PRIVATE -ffp-contract=off -frounding-math)
    endif()
endif()
