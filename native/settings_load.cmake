if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_settings_dialog)
    add_library(nijigenerate_settings_load STATIC src/nijigenerate/core/settings_load.cpp)
    target_compile_features(nijigenerate_settings_load PUBLIC cxx_std_20)
    target_sources(nijigenerate_settings_load PUBLIC FILE_SET settings_load_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_read_text.hpp")
    if(WIN32)
        target_sources(nijigenerate_settings_load PRIVATE src/nijigenerate/platform/settings_file_windows.cpp)
    else()
        target_sources(nijigenerate_settings_load PRIVATE src/nijigenerate/platform/settings_file_portable.cpp)
    endif()
    target_link_libraries(nijigenerate_settings_load PUBLIC
        nijigenerate_runtime_core nijigenerate_settings_dialog
        PRIVATE nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_settings_load PRIVATE /fp:strict)
        set_source_files_properties(src/nijigenerate/core/settings_save.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        target_compile_options(nijigenerate_settings_load PRIVATE -ffp-contract=off -frounding-math)
        set_source_files_properties(src/nijigenerate/core/settings_save.cpp
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-frounding-math")
    endif()
endif()
