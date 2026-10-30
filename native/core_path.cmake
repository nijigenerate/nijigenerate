# One real path/environment provider for ordinary and legacy runtime consumers.
if(TARGET nijigenerate_native_options AND NOT TARGET nijigenerate_core_path)
    add_library(nijigenerate_core_path STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/path.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/environment.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/path_policy.cpp")
    target_sources(nijigenerate_core_path PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/detail/path_allocation.hpp")
    target_compile_features(nijigenerate_core_path PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_core_path PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_core_path PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/path.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/environment.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/path_policy.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_exists.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_directory.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_windows_exception.hpp")
    target_link_libraries(nijigenerate_core_path PUBLIC nijigenerate_native_options
        PRIVATE nijigenerate_phobos_stdio)
    if(TARGET nijigenerate_core)
        target_link_libraries(nijigenerate_core PRIVATE nijigenerate_core_path)
    endif()
endif()
