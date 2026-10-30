# Configure target policy at build time so the formatter remains consteval.
function(nijigenerate_configure_shortcut_policy target visibility)
    set(NIJIGENERATE_SHORTCUT_APPLE false)
    if(APPLE)
        set(NIJIGENERATE_SHORTCUT_APPLE true)
    endif()
    set(policy_root "${CMAKE_CURRENT_BINARY_DIR}/shortcut-policy")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shortcut_policy.hpp.in"
        "${policy_root}/nijigenerate/platform/shortcut_policy.hpp" @ONLY)
    target_include_directories(${target} ${visibility} "${policy_root}")
endfunction()
