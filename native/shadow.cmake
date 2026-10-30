add_library(nijigenerate_shadow STATIC src/nijigenerate/widgets/shadow.cpp)
set(NIJIGENERATE_IMGUI_BINDING_SOURCE "" CACHE FILEPATH "Current independent D ImGui binding declaration for shadow differential")
target_include_directories(nijigenerate_shadow PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(nijigenerate_shadow PUBLIC nijigenerate_imgui nijigenerate_native_options)
target_sources(nijigenerate_shadow PUBLIC
    FILE_SET shadow_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/shadow.hpp")
if(BUILD_TESTING)
    add_library(nijigenerate_shadow_header_probe OBJECT tests/shadow_header_probe.cpp)
    target_link_libraries(nijigenerate_shadow_header_probe PRIVATE nijigenerate_shadow)
    add_executable(nijigenerate_shadow_imgui_probe tests/shadow_imgui_probe.cpp)
    target_link_libraries(nijigenerate_shadow_imgui_probe PRIVATE nijigenerate_shadow)
    add_test(NAME native_shadow_imgui_contract COMMAND nijigenerate_shadow_imgui_probe)
    add_executable(nijigenerate_shadow_lifetime_probe tests/shadow_lifetime_probe.cpp)
    target_link_libraries(nijigenerate_shadow_lifetime_probe PRIVATE nijigenerate_shadow)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_IMGUI_BINDING_SOURCE)
        add_test(NAME native_shadow_lifetime_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_shadow_lifetime.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --imgui-binding "${NIJIGENERATE_IMGUI_BINDING_SOURCE}"
                --native "$<TARGET_FILE:nijigenerate_shadow_lifetime_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/shadow-lifetime-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_shadow_project DEPENDS nijigenerate_shadow
        nijigenerate_shadow_header_probe nijigenerate_shadow_imgui_probe nijigenerate_shadow_lifetime_probe)
endif()
