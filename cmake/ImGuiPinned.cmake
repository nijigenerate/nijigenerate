# Official v1.91.9b-docking archive; configure never downloads dependencies.
set(NIJIGENERATE_IMGUI_SOURCE_DIR "" CACHE PATH "Local Dear ImGui v1.91.9b-docking sources")
set(imgui_pin
    imgui.h EA876E477F06F9619206284551638171C7BA4ED4DBC6BA67002E8F2E4AD7D3C7
    imgui.cpp 2FAF54DF0DB896FCB30B8E528E064F2C3227F2F695FE1C8FE62AA8B71F228084
    imgui_demo.cpp E16CC8042CF673BD3AE5CF6F4E88485D7A6E3A06B9F64ADCF4C32371C13605B9
    imgui_draw.cpp 8823F49F5C7A5181C0435A5FA01A03C3FF5BECE283482B81001E71A66993F997
    imgui_tables.cpp 4EA380FA9D49CAC6CBFBC8B969969F9B0705518EC9CA340681D8C70E3EEFBC28
    imgui_widgets.cpp 215EAE588D52B455344AB5BD9847AB133D8BAFFF70841F992F7701B9FBDA4DBC
    imgui_internal.h B6A9B40031C95C83D27595CE0B71F7E102D705CCAC6BEE6D03FF931FDD370569
    imconfig.h 0884061CC43E27E1D346FE18DCD1425792EB2532D3D16610A82EB5AC751F7594
    imstb_rectpack.h 2EFA3D5F7D003C19743B15155DAD9F46D9F9FC783A18893D703B431D3C990972
    imstb_textedit.h 77010E494CAF72C178B573569612AEC409D24380409676325A31D02DE0EC64B7
    imstb_truetype.h 87F08919AE8E7223451D0E9D06B62829075D3E920C9E6AD436DB9D7E7427FFCE
    misc/freetype/imgui_freetype.cpp D84902BBFB0CCB5BE3BC106BF4942195931D7620A400B2C03D79B224F810B625
    misc/freetype/imgui_freetype.h 7672653524074AA719E4ED04B26FB46EA2DF894C6585D9CDA8747A6DB60BABE3
    backends/imgui_impl_sdl3.cpp 6C2DB7ECB087695951198039960B2EA3B28682CBB27E672C724A0902403D43C5
    backends/imgui_impl_sdl3.h 76CBFD744F27D26C7E17F25BE2E685598B5CA31D3B336DB8AF32C3972B323C01)
while(imgui_pin)
    list(POP_FRONT imgui_pin filename expected)
    if(NOT EXISTS "${NIJIGENERATE_IMGUI_SOURCE_DIR}/${filename}")
        message(FATAL_ERROR "Pinned ImGui source missing: ${filename}; provide v1.91.9b-docking locally")
    endif()
    file(SHA256 "${NIJIGENERATE_IMGUI_SOURCE_DIR}/${filename}" actual)
    string(TOUPPER "${actual}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Pinned ImGui source mismatch: ${filename}")
    endif()
endwhile()
find_package(SDL3 3.2.28 EXACT CONFIG REQUIRED)
add_library(nijigenerate_imgui STATIC
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui.cpp"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui_demo.cpp"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/imgui_widgets.cpp")
target_include_directories(nijigenerate_imgui PUBLIC "${NIJIGENERATE_IMGUI_SOURCE_DIR}")
target_compile_features(nijigenerate_imgui PUBLIC cxx_std_20)
# Preserve the unchanged baseline, excluded from ordinary application builds.
# Its raw implementation must never be linked with the project platform adapter.
add_library(nijigenerate_imgui_sdl3_reference STATIC EXCLUDE_FROM_ALL
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
target_include_directories(nijigenerate_imgui_sdl3_reference PUBLIC "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends")
target_link_libraries(nijigenerate_imgui_sdl3_reference PUBLIC nijigenerate_imgui SDL3::SDL3)
set_property(TARGET nijigenerate_imgui_sdl3_reference PROPERTY
    INTERFACE_NIJIGENERATE_SDL3_PLATFORM_VARIANT "raw-reference")
set_property(TARGET nijigenerate_imgui_sdl3_reference APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING NIJIGENERATE_SDL3_PLATFORM_VARIANT)

add_library(nijigenerate_imgui_sdl3 STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../native/src/nijigenerate/platform/imgui_sdl3_adapter.cpp")
target_include_directories(nijigenerate_imgui_sdl3 PUBLIC
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends" "${CMAKE_CURRENT_LIST_DIR}/../native/src")
target_link_libraries(nijigenerate_imgui_sdl3 PUBLIC nijigenerate_imgui SDL3::SDL3)
target_sources(nijigenerate_imgui_sdl3 PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../native/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/../native/src/nijigenerate/platform/imgui_sdl3_adapter.hpp")
set_property(TARGET nijigenerate_imgui_sdl3 PROPERTY
    INTERFACE_NIJIGENERATE_SDL3_PLATFORM_VARIANT "project-adapter")
set_property(TARGET nijigenerate_imgui_sdl3 APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING NIJIGENERATE_SDL3_PLATFORM_VARIANT)
