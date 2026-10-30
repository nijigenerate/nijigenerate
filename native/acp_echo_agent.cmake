if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_phobos_stdio
    AND NOT TARGET nijigenerate_acp_echo_agent)
    add_executable(nijigenerate_acp_echo_agent EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/echo_agent.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/echo_agent.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/detail/echo_response.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_conv_exception.hpp")
    target_compile_definitions(nijigenerate_acp_echo_agent PRIVATE ACP_ECHO_TOOL=1)
    target_link_libraries(nijigenerate_acp_echo_agent PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nijigenerate_phobos_stdio)
    if(MSVC)
        target_compile_options(nijigenerate_acp_echo_agent PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_acp_echo_agent PRIVATE -ffp-contract=off)
    endif()
endif()
