# M170 is one source-file compilation group. Mutation/editor and policy providers
# remain explicit prerequisites; this registration does not certify completeness.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imgui AND TARGET nijigenerate_drag
        AND TARGET nijigenerate_dummy AND TARGET nijigenerate_inputtext AND TARGET nijigenerate_core
        AND TARGET nijigenerate_category)
    add_library(nijigenerate_inspector_common STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common.cpp")
    target_compile_features(nijigenerate_inspector_common PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_inspector_common PUBLIC nijigenerate_runtime_core
        nijigenerate_imgui nijigenerate_drag nijigenerate_dummy nijigenerate_core
        PRIVATE nijigenerate_inputtext nijigenerate_category nijigenerate_mcp_queue)
    if(MSVC)
        if(NOT TARGET Boost::multiprecision)
            find_package(Boost CONFIG REQUIRED COMPONENTS multiprecision)
        endif()
        target_link_libraries(nijigenerate_inspector_common PRIVATE Boost::multiprecision)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    target_sources(nijigenerate_inspector_common PUBLIC
        FILE_SET inspector_common_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/common_object.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_inspector_enums.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/selection.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/shared_delegate.hpp")
    if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
        add_library(nijigenerate_inspector_common_header_probe OBJECT
            "${CMAKE_CURRENT_LIST_DIR}/tests/inspector_common_header_probe.cpp")
        target_link_libraries(nijigenerate_inspector_common_header_probe PRIVATE nijigenerate_inspector_common)
        add_executable(nijigenerate_inspector_common_ownership
            "${CMAKE_CURRENT_LIST_DIR}/tests/inspector_common_ownership_probe.cpp")
        target_link_libraries(nijigenerate_inspector_common_ownership PRIVATE nijigenerate_inspector_common)
        add_test(NAME native_inspector_common_ownership COMMAND nijigenerate_inspector_common_ownership)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_inspector_common_ownership_original
                COMMAND "${NIJIGENERATE_PYTHON}"
                    "${CMAKE_CURRENT_LIST_DIR}/tests/compare_inspector_common_ownership.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_inspector_common_ownership>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/inspector-common-ownership/$<CONFIG>")
        endif()
    endif()
endif()
