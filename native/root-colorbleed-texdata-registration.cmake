target_sources(nijigenerate_core PRIVATE src/nijigenerate/core/colorbleed_texdata.hpp)
include("${CMAKE_CURRENT_LIST_DIR}/colorbleed_publication.cmake")
ng_publish_colorbleed()
if(BUILD_TESTING)
    if(NOT TARGET nijigenerate_colorbleed_types_probe)
        include("${CMAKE_CURRENT_LIST_DIR}/colorbleed.cmake")
    endif()
    add_executable(nijigenerate_colorbleed_texdata_probe tests/colorbleed_texdata_probe.cpp)
    target_link_libraries(nijigenerate_colorbleed_texdata_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_colorbleed_texdata_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_colorbleed_texdata.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_colorbleed_texdata_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/colorbleed-texdata-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()
