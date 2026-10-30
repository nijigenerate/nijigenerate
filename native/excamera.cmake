if(TARGET nijigenerate_debug AND TARGET nijigenerate_label AND TARGET nijigenerate_core
        AND TARGET nicxlive::nicxlive AND NOT TARGET nijigenerate_excamera)
    add_library(nijigenerate_excamera STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/excamera.cpp")
    target_include_directories(nijigenerate_excamera PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_excamera PUBLIC cxx_std_20)
    target_sources(nijigenerate_excamera PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/excamera.hpp")
    target_sources(nijigenerate_excamera PRIVATE FILE_SET excamera_math_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/excamera_math.hpp")
    target_link_libraries(nijigenerate_excamera PUBLIC nijigenerate_debug nijigenerate_label
        nijigenerate_core nicxlive::nicxlive)
    if(MSVC)
        target_compile_options(nijigenerate_excamera PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_excamera PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
