# Include after runtime project, button and texture_widget registrations.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_button AND TARGET nijigenerate_texture_widget
        AND TARGET nijigenerate_imgui AND TARGET nicxlive::nicxlive
        AND NOT TARGET nijigenerate_automesh_alpha_provider)
    add_library(nijigenerate_automesh_alpha_provider STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/alpha_provider.cpp")
    target_include_directories(nijigenerate_automesh_alpha_provider PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_automesh_alpha_provider PUBLIC
        FILE_SET alpha_provider_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/alpha_provider.hpp")
    target_compile_features(nijigenerate_automesh_alpha_provider PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_alpha_provider PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_alpha_provider PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    target_link_libraries(nijigenerate_automesh_alpha_provider PUBLIC nicxlive::nicxlive
        nijigenerate_runtime_core nijigenerate_button nijigenerate_texture_widget nijigenerate_imgui)
endif()
