if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_settings_load
        AND TARGET nijigenerate_settings_dialog AND TARGET Boost::json
        AND NOT TARGET nijigenerate_settings)
    add_library(nijigenerate_settings INTERFACE)
    target_include_directories(nijigenerate_settings INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_settings INTERFACE cxx_std_20)
    target_sources(nijigenerate_settings INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_value.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_value_metadata.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_string.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_string_map.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_enum.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_inspector_enums.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_dialog.hpp")
    target_link_libraries(nijigenerate_settings INTERFACE
        nijigenerate_runtime_core nijigenerate_settings_load nijigenerate_settings_dialog Boost::json)
endif()
