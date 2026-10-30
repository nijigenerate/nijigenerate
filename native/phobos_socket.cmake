if(TARGET nijigenerate_native_options AND TARGET nijigenerate_runtime_core
        AND NOT TARGET nijigenerate_phobos_socket)
    add_library(nijigenerate_phobos_socket STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_socket.cpp")
    target_compile_features(nijigenerate_phobos_socket PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_phobos_socket PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_phobos_socket PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_socket.hpp")
    target_link_libraries(nijigenerate_phobos_socket PUBLIC nijigenerate_native_options nijigenerate_runtime_core)
    if(WIN32)
        target_compile_definitions(nijigenerate_phobos_socket PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(nijigenerate_phobos_socket PRIVATE ws2_32)
    endif()
endif()
