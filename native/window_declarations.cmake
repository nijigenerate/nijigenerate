if(NIJIGENERATE_IMGUI AND TARGET nicxlive::nicxlive AND TARGET SDL3::SDL3)
    add_library(nijigenerate_window_declarations INTERFACE)
    target_compile_features(nijigenerate_window_declarations INTERFACE cxx_std_20)
    target_sources(nijigenerate_window_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/window.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/window_platform.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/window_theme.hpp")
    target_include_directories(nijigenerate_window_declarations INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/third_party")
    target_link_libraries(nijigenerate_window_declarations INTERFACE
        nijigenerate_native_options SDL3::SDL3 nijigenerate_imgui Boost::json)
    add_library(nijigenerate_window_state STATIC src/nijigenerate/core/window_state.cpp)
    target_compile_features(nijigenerate_window_state PUBLIC cxx_std_20)
    target_sources(nijigenerate_window_state PRIVATE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/window_state_internal.hpp")
    target_include_directories(nijigenerate_window_state PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_window_state PUBLIC
        nijigenerate_native_options SDL3::SDL3 nijigenerate_imgui)
    target_link_libraries(nijigenerate_window_declarations INTERFACE nijigenerate_window_state)
    set(window_sdl3_binding_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/window-sdl3/$<CONFIG>")
    file(GENERATE
        OUTPUT "${window_sdl3_binding_directory}/nijigenerate/platform/generated/window_sdl3_binding.hpp"
        CONTENT "#pragma once\nnamespace nijigenerate::platform::detail {\ninline constexpr char selectedSDL3TargetFile[] = R\"ngSDL3($<TARGET_FILE:SDL3::SDL3>)ngSDL3\";\ninline constexpr char selectedSDL3TargetType[] = \"$<TARGET_PROPERTY:SDL3::SDL3,TYPE>\";\n}\n")
    add_library(nijigenerate_window_sdl3_binding STATIC src/nijigenerate/platform/window_sdl3_binding.cpp)
    target_compile_features(nijigenerate_window_sdl3_binding PUBLIC cxx_std_20)
    target_sources(nijigenerate_window_sdl3_binding PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/platform/window_sdl3_binding.hpp")
    target_include_directories(nijigenerate_window_sdl3_binding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
        PRIVATE "${window_sdl3_binding_directory}")
    target_link_libraries(nijigenerate_window_sdl3_binding PUBLIC nijigenerate_native_options SDL3::SDL3)
    target_link_libraries(nijigenerate_window_declarations INTERFACE nijigenerate_window_sdl3_binding)
    if(WIN32)
        add_library(nijigenerate_window_win32_dpi STATIC src/nijigenerate/platform/window_win32_dpi.cpp)
        target_compile_features(nijigenerate_window_win32_dpi PUBLIC cxx_std_20)
        target_sources(nijigenerate_window_win32_dpi PUBLIC FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_win32_dpi.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/window_win32_dpi.hpp")
        target_include_directories(nijigenerate_window_win32_dpi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(nijigenerate_window_win32_dpi PUBLIC nijigenerate_native_options SDL3::SDL3)
        target_link_libraries(nijigenerate_window_declarations INTERFACE nijigenerate_window_win32_dpi)
    endif()
    add_library(nijigenerate_window_simple STATIC src/nijigenerate/core/window_simple.cpp
        src/nijigenerate/core/window_layout.cpp)
    target_compile_features(nijigenerate_window_simple PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_window_simple PUBLIC nijigenerate_window_declarations)
    add_library(nijigenerate_window_finalize STATIC src/nijigenerate/core/window_finalize.cpp)
    # The actual renderer target is declared later in the parent CMake source.
    target_link_libraries(nijigenerate_window_finalize PUBLIC nijigenerate_window_declarations
        PRIVATE nijigenerate_gl_backend_source nijigenerate_imgui_sdl3)
    if(TARGET nijigenerate_runtime_core)
        # Finalize directly calls the real MCP and AutoMesh shutdown bodies.
        target_link_libraries(nijigenerate_window_finalize PRIVATE nijigenerate_runtime_core)
    endif()
    if(NIJIGENERATE_RUNTIME_PROBE AND NIJIGENERATE_EMBED_RESOURCES AND NIJIGENERATE_DIALOGS)
        add_library(nijigenerate_window_context STATIC src/nijigenerate/core/window_context.cpp
            src/nijigenerate/platform/window_context_policy.cpp)
        target_link_libraries(nijigenerate_window_context PUBLIC nijigenerate_window_declarations
            PRIVATE nijigenerate_runtime_core nijigenerate_core_path
                nijigenerate_window_theme_settings nijigenerate_window_simple
                nijigenerate_dpi
                nijigenerate_font nijigenerate_dialogs nijigenerate_window_background
                nijigenerate_gl_backend_source nijigenerate_imgui_sdl3)
        if(APPLE)
            target_compile_definitions(nijigenerate_window_context PRIVATE NG_OSX)
        endif()
        if(WIN32 AND NIJIGENERATE_LOGO_RESOURCES)
            add_library(nijigenerate_window_open_win32 STATIC src/nijigenerate/platform/window_open_win32.cpp)
            target_link_libraries(nijigenerate_window_open_win32 PUBLIC nijigenerate_window_declarations
                PRIVATE nijigenerate_window_factory nijigenerate_window_context nijigenerate_window_win32_dpi
                    nijigenerate_window_sdl3_binding nijigenerate_runtime_core nijigenerate_gl_backend_source
                    nijigenerate_logo)
            target_compile_definitions(nijigenerate_window_open_win32 PRIVATE
                $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
        endif()
        if(APPLE AND NIJIGENERATE_LOGO_RESOURCES)
            add_library(nijigenerate_window_open_macos STATIC src/nijigenerate/platform/window_open_macos.cpp)
            target_link_libraries(nijigenerate_window_open_macos PUBLIC nijigenerate_window_declarations
                PRIVATE nijigenerate_window_factory nijigenerate_window_context
                    nijigenerate_window_sdl3_binding nijigenerate_runtime_core nijigenerate_gl_backend_source
                    nijigenerate_logo nijigenerate_core nijigenerate_window_debug)
            target_compile_definitions(nijigenerate_window_open_macos PRIVATE
                $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
        endif()
    endif()
    if(TARGET nijigenerate_runtime_core)
        add_library(nijigenerate_window_factory STATIC src/nijigenerate/core/window_factory.cpp
            src/nijigenerate/platform/window_factory.cpp)
        target_link_libraries(nijigenerate_window_factory PUBLIC nijigenerate_window_declarations
            PRIVATE nijigenerate_runtime_core)
    endif()
    option(NIJIGENERATE_WINDOW_DEBUG_IMGUI "Enable source debug(imgui) window diagnostics" OFF)
    add_library(nijigenerate_window_debug STATIC src/nijigenerate/core/window_debug.cpp)
    target_link_libraries(nijigenerate_window_debug PUBLIC nijigenerate_window_declarations)
    target_link_libraries(nijigenerate_window_debug PRIVATE nijigenerate_core nijigenerate_phobos_stdio)
    target_sources(nijigenerate_window_debug PRIVATE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_diagnostic_internal.hpp")
    target_compile_definitions(nijigenerate_window_debug PRIVATE
        $<$<BOOL:${NIJIGENERATE_WINDOW_DEBUG_IMGUI}>:NIJIGENERATE_WINDOW_DEBUG_IMGUI=1>
        $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
    if(BUILD_TESTING)
        add_library(nijigenerate_window_header_probe OBJECT tests/window_header_a.cpp tests/window_header_b.cpp)
        target_link_libraries(nijigenerate_window_header_probe PRIVATE nijigenerate_window_declarations)
        add_custom_target(nijigenerate_window_header_project DEPENDS nijigenerate_window_header_probe)
    endif()
endif()
