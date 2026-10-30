target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage.cpp)
target_sources(nijigenerate_core PUBLIC
    FILE_SET depthimage_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthimage.hpp")
target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_psd_settings.cpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_depthimage_psd_settings_probe tests/depthimage_psd_settings_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_psd_settings_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_psd_settings_probe tests/psd_settings_probe.cpp)
    target_link_libraries(nijigenerate_psd_settings_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthimage_header_probe tests/depthimage_header_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_header_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthimage_settings_probe tests/depthimage_settings_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_settings_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthimage_sampling_probe tests/depthimage_sampling_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_sampling_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depthimage_psd_settings_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_psd_settings.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_psd_settings_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-psd-settings-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
        add_test(NAME native_psd_settings_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/observe_psd_settings.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_psd_settings_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psd-settings-evidence/$<CONFIG>")
        add_test(NAME native_depthimage_header_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_header.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_header_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-header-evidence/$<CONFIG>")
        add_test(NAME native_depthimage_settings_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_settings.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_settings_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-settings-evidence/$<CONFIG>")
        add_test(NAME native_depthimage_sampling_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_sampling.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_sampling_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-sampling-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()

target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_rgba_decode.cpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_depthimage_rgba_decode_probe tests/depthimage_rgba_decode_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_rgba_decode_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depthimage_rgba_decode_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_rgba_decode.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_rgba_decode_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-rgba-decode-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()

target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_binary_mask.cpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_depthimage_binary_mask_probe tests/depthimage_binary_mask_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_binary_mask_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depthimage_binary_mask_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_binary_mask.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_binary_mask_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-binary-mask-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()

target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_layer_scale.cpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_depthimage_layer_scale_probe tests/depthimage_layer_scale_probe.cpp)
    target_link_libraries(nijigenerate_depthimage_layer_scale_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depthimage_layer_scale_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthimage_layer_scale.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthimage_layer_scale_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthimage-layer-scale-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()
