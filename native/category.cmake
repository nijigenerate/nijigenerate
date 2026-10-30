if(NIJIGENERATE_IMGUI)
    add_library(nijigenerate_category STATIC src/nijigenerate/widgets/category_readonly.cpp
        src/nijigenerate/widgets/category_colors.cpp)
    target_include_directories(nijigenerate_category PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(nijigenerate_category PUBLIC cxx_std_20)
    target_sources(nijigenerate_category PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/category.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/category_flags.hpp"
        PRIVATE FILE_SET category_internal_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/category_internal.hpp")
    target_link_libraries(nijigenerate_category PUBLIC nijigenerate_imgui nijigenerate_native_options)
    if(BUILD_TESTING)
        add_library(nijigenerate_category_header_probe OBJECT tests/category_header_probe.cpp)
        target_link_libraries(nijigenerate_category_header_probe PRIVATE nijigenerate_category)
        add_executable(nijigenerate_category_imgui_probe tests/category_imgui_probe.cpp)
        target_link_libraries(nijigenerate_category_imgui_probe PRIVATE nijigenerate_category)
        add_executable(nijigenerate_category_readonly_recording_probe tests/category_readonly_recording_probe.cpp
            src/nijigenerate/widgets/category_readonly.cpp)
        add_executable(nijigenerate_category_data_recording_probe tests/category_data_recording_probe.cpp
            src/nijigenerate/widgets/category_colors.cpp)
        foreach(category_probe IN ITEMS nijigenerate_category_readonly_recording_probe nijigenerate_category_data_recording_probe)
            target_include_directories(${category_probe} PRIVATE tests/category_recording src)
            target_link_libraries(${category_probe} PRIVATE nijigenerate_native_options)
        endforeach()
        add_custom_target(nijigenerate_category_project DEPENDS nijigenerate_category
            nijigenerate_category_header_probe nijigenerate_category_imgui_probe
            nijigenerate_category_readonly_recording_probe nijigenerate_category_data_recording_probe)
        add_test(NAME native_category_helpers_imgui COMMAND nijigenerate_category_imgui_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
            foreach(category_part IN ITEMS readonly data)
                add_test(NAME native_category_${category_part}_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_button_color.py"
                        --widget "category_${category_part}" --compiler "${NIJIGENERATE_D_COMPILER}"
                        --native "$<TARGET_FILE:nijigenerate_category_${category_part}_recording_probe>"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/category-${category_part}-differential/$<CONFIG>")
            endforeach()
        endif()
    endif()
endif()
