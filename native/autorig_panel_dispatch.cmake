include_guard(GLOBAL)
if(NIJIGENERATE_RUNTIME_PROBE)
    if(NOT TARGET nijigenerate_runtime_core)
        message(FATAL_ERROR "Register AutoRig panel dispatch after the actual runtime core target")
    endif()
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/detail/autorig_dispatch.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/autorig_dispatch.cpp")
endif()
