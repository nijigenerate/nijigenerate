# The owning application catalogue includes this after the real ACP client,
# Panel framework and widget providers. This fragment does not register a
# substitute panel or invent missing module lifecycle definitions.
if(TARGET nijigenerate_acp_client AND TARGET nijigenerate_panels
        AND TARGET nijigenerate_inputtext AND TARGET nijigenerate_button
        AND NOT TARGET nijigenerate_agent_panel)
    # The catalogue already owns agent_lifecycle.cpp on runtime_core.
    add_library(nijigenerate_agent_panel STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/agent_panel.cpp")
    target_include_directories(nijigenerate_agent_panel PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_agent_panel PUBLIC cxx_std_20)
    target_compile_definitions(nijigenerate_agent_panel PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(nijigenerate_agent_panel PUBLIC nijigenerate_panels nijigenerate_acp_client
        nijigenerate_runtime_core nijigenerate_core nijigenerate_native_options
        PRIVATE nijigenerate_inputtext nijigenerate_button)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_agent_panel)
    target_sources(nijigenerate_agent_panel PUBLIC
        FILE_SET agent_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/agent.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/agent_panel.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/agent_permission.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_agent_panel PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_agent_panel PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
