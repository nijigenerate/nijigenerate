if(BUILD_TESTING)
    foreach(bleed_kind IN ITEMS types mask iteration)
        add_executable(nijigenerate_colorbleed_${bleed_kind}_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/colorbleed_${bleed_kind}_probe.cpp")
        target_compile_features(nijigenerate_colorbleed_${bleed_kind}_probe PRIVATE cxx_std_20)
        target_include_directories(nijigenerate_colorbleed_${bleed_kind}_probe PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src")
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_colorbleed_${bleed_kind}_differential
                COMMAND "${NIJIGENERATE_PYTHON}"
                    "${CMAKE_CURRENT_LIST_DIR}/tests/compare_colorbleed_${bleed_kind}.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_colorbleed_${bleed_kind}_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/colorbleed-${bleed_kind}-evidence/$<CONFIG>")
        endif()
    endforeach()
endif()
