if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psdlayers.cpp")
    target_compile_features(nijigenerate_core PUBLIC cxx_std_20)
    target_sources(nijigenerate_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psdlayers.hpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psdlayers.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psdlayers.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
