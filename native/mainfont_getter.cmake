if(BUILD_TESTING AND TARGET nijigenerate_core_package)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_mainfont_getter_probe tests/mainfont_getter_probe.cpp)
    target_link_libraries(nijigenerate_mainfont_getter_probe PRIVATE
        nijigenerate_core_package nijigenerate_core Threads::Threads)
    if(MSVC)
        target_compile_options(nijigenerate_mainfont_getter_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        find_path(NIJIGENERATE_D_IMGUI_SOURCE_DIR bindbc/imgui/bind/imgui.d
            HINTS "$ENV{LOCALAPPDATA}/dub/packages/i2d-imgui/0.8.0/i2d-imgui/source")
        if(NIJIGENERATE_D_IMGUI_SOURCE_DIR)
            add_test(NAME native_mainfont_getter_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_mainfont_getter.py"
                    --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                    --native-imgui-header "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui.h"
                    --native "$<TARGET_FILE:nijigenerate_mainfont_getter_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/mainfont-getter-evidence/$<CONFIG>")
        else()
            message(WARNING "Main font differential requires actual i2d-imgui D binding source; not verified")
        endif()
    endif()
endif()
