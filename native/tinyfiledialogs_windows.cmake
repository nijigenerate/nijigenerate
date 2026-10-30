if(WIN32 AND NOT TARGET nijigenerate_tinyfiledialogs)
    add_library(nijigenerate_tinyfiledialogs STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs_windows.cpp")
    target_include_directories(nijigenerate_tinyfiledialogs PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_tinyfiledialogs PUBLIC cxx_std_20)
    target_compile_definitions(nijigenerate_tinyfiledialogs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_sources(nijigenerate_tinyfiledialogs PUBLIC FILE_SET tinyfiledialogs_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs.hpp")
    target_link_libraries(nijigenerate_tinyfiledialogs PUBLIC comdlg32 ole32 user32 shell32)
endif()
