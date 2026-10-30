option(NIJIGENERATE_OSQP_INTEGRATION "Expose the existing local fixed-ABI OSQP bridge" OFF)
set(NIJIGENERATE_OSQP_ROOT "${PROJECT_SOURCE_DIR}/out/osqp" CACHE PATH "Installed local OSQP bridge prefix")
if(NIJIGENERATE_OSQP_INTEGRATION)
    find_library(NG_OSQP_BRIDGE NAMES nijigenerate_osqp
        PATHS "${NIJIGENERATE_OSQP_ROOT}/lib" NO_DEFAULT_PATH NO_CACHE)
    find_library(NG_OSQP_STATIC NAMES osqpstatic
        PATHS "${NIJIGENERATE_OSQP_ROOT}/lib" NO_DEFAULT_PATH NO_CACHE)
    if(NOT NG_OSQP_BRIDGE OR NOT NG_OSQP_STATIC)
        message(FATAL_ERROR "NIJIGENERATE_OSQP_ROOT must contain existing local nijigenerate_osqp and osqpstatic archives")
    endif()
    add_library(nijigenerate_osqp_static STATIC IMPORTED GLOBAL)
    set_target_properties(nijigenerate_osqp_static PROPERTIES IMPORTED_LOCATION "${NG_OSQP_STATIC}")
    add_library(nijigenerate_osqp_bridge STATIC IMPORTED GLOBAL)
    set_target_properties(nijigenerate_osqp_bridge PROPERTIES
        IMPORTED_LOCATION "${NG_OSQP_BRIDGE}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/src"
        INTERFACE_LINK_LIBRARIES nijigenerate_osqp_static)
    add_library(nijigenerate::osqp_bridge ALIAS nijigenerate_osqp_bridge)
    if(BUILD_TESTING)
        add_executable(nijigenerate_osqp_abi_probe "${CMAKE_CURRENT_LIST_DIR}/tests/osqp_abi_probe.cpp")
        target_compile_features(nijigenerate_osqp_abi_probe PRIVATE cxx_std_20)
        target_link_libraries(nijigenerate_osqp_abi_probe PRIVATE nijigenerate::osqp_bridge)
        # Existing local solver archives use the release DLL CRT, also in Debug probes.
        if(MSVC)
            set_property(TARGET nijigenerate_osqp_abi_probe PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_custom_target(nijigenerate_osqp_project DEPENDS nijigenerate_osqp_abi_probe)
        add_test(NAME native_osqp_abi_contract COMMAND nijigenerate_osqp_abi_probe)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_osqp_abi_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_osqp_abi.py"
                    "$<TARGET_FILE:nijigenerate_osqp_abi_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/osqp-evidence/$<CONFIG>"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --prefix "${NIJIGENERATE_OSQP_ROOT}")
        endif()
    endif()
endif()
