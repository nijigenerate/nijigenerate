# Actual module TLS, registration and panel class bodies; missing real providers
# remain explicit source frontiers, without replacement stubs.
if(TARGET nijigenerate_autorig_panel_declarations AND TARGET nijigenerate_autorig_framework
        AND TARGET nijigenerate_button AND TARGET nijigenerate_inputtext
        AND TARGET nijigenerate_mcp_queue AND TARGET nijigenerate_crashdump
        AND TARGET nijigenerate_autorig_evidence AND TARGET nijigenerate_autorig_review
        AND NOT TARGET nijigenerate_autorig_panel_registration)
    add_library(nijigenerate_autorig_panel_registration STATIC
        src/nijigenerate/panels/autorig_registration.cpp
        src/nijigenerate/panels/autorig_cleanup.cpp
        src/nijigenerate/panels/autorig_module.cpp
        src/nijigenerate/panels/autorig_dispatch.cpp
        src/nijigenerate/panels/autorig_lifecycle.cpp
        src/nijigenerate/panels/detail/autorig_worker.cpp
        src/nijigenerate/panels/autorig_panel.cpp)
    find_package(Threads REQUIRED)
    target_compile_features(nijigenerate_autorig_panel_registration PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_panel_registration PUBLIC
        nijigenerate_autorig_panel_declarations nijigenerate_autorig_framework
        PRIVATE nijigenerate_core nijigenerate_button nijigenerate_inputtext
            nijigenerate_mcp_queue nijigenerate_crashdump Threads::Threads
            nijigenerate_autorig_evidence nijigenerate_autorig_review)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_panel_registration PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_panel_registration PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    target_sources(nijigenerate_autorig_panel_registration PRIVATE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/detail/autorig_sessions.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/detail/autorig_worker.hpp")
endif()
if(TARGET nijigenerate_autorig_panel_registration AND TARGET nijigenerate_autorig_processor
        AND TARGET nijigenerate_autorig_editor)
    target_sources(nijigenerate_autorig_panel_registration PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/autorig_bootstrap.cpp")
    target_link_libraries(nijigenerate_autorig_panel_registration PRIVATE
        nijigenerate_autorig_processor nijigenerate_autorig_editor)
endif()
