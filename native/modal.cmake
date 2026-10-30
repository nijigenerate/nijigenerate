add_library(nijigenerate_modal STATIC src/nijigenerate/widgets/modal.cpp)
target_include_directories(nijigenerate_modal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_modal PUBLIC cxx_std_20)
target_sources(nijigenerate_modal PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal/base.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal.hpp")
target_link_libraries(nijigenerate_modal PUBLIC
    nijigenerate_nagscreen_declarations nijigenerate_imgui nijigenerate_native_options nicxlive::nicxlive)
if(BUILD_TESTING)
    add_library(nijigenerate_modal_header_probe OBJECT tests/modal_header_probe.cpp
        tests/modal_umbrella_probe.cpp)
    target_link_libraries(nijigenerate_modal_header_probe PRIVATE nijigenerate_modal)
    add_executable(nijigenerate_modal_runtime_probe tests/modal_runtime_probe.cpp)
    target_link_libraries(nijigenerate_modal_runtime_probe PRIVATE nijigenerate_modal)
    add_test(NAME native_modal_real_imgui_runtime COMMAND nijigenerate_modal_runtime_probe)
    add_custom_target(nijigenerate_modal_project DEPENDS nijigenerate_modal_header_probe
        nijigenerate_modal_runtime_probe)
endif()
