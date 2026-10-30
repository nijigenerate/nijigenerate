if(UNIX AND NOT TARGET nijigenerate_crashdump_posix_state)
    add_library(nijigenerate_crashdump_posix_state STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/crashdump_posix_state.cpp")
    target_include_directories(nijigenerate_crashdump_posix_state PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_crashdump_posix_state PUBLIC cxx_std_20)
endif()
