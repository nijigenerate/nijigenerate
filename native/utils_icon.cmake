target_sources(nijigenerate_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils.cpp")
target_sources(nijigenerate_core PUBLIC
    FILE_SET utils_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils.hpp")
if(BUILD_TESTING)
    add_executable(nijigenerate_utils_icon_probe "${CMAKE_CURRENT_LIST_DIR}/tests/utils_icon_probe.cpp")
    target_link_libraries(nijigenerate_utils_icon_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_utils_icon_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_utils_icon.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_utils_icon_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/utils-icon-evidence/$<CONFIG>")
    endif()
endif()
