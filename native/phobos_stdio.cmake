if(NOT TARGET nijigenerate_phobos_stdio)
    add_library(nijigenerate_phobos_stdio STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_stdio.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_stdio.hpp")
    target_include_directories(nijigenerate_phobos_stdio PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_phobos_stdio PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_phobos_stdio PUBLIC nijigenerate_native_options)
endif()
