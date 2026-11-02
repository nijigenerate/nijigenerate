if(BUILD_TESTING)
    add_library(nijigenerate_depth_internal_declarations OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/tests/depth_internal_declarations_a.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/depth_internal_declarations_b.cpp")
    target_include_directories(nijigenerate_depth_internal_declarations PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_depth_internal_declarations PRIVATE cxx_std_20)
    add_executable(nijigenerate_depth_internal_types_probe "${CMAKE_CURRENT_LIST_DIR}/tests/depth_internal_types_probe.cpp")
    target_include_directories(nijigenerate_depth_internal_types_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_depth_internal_types_probe PRIVATE cxx_std_20)
    add_custom_target(nijigenerate_depthimage_internal_types_project DEPENDS nijigenerate_depth_internal_declarations
        nijigenerate_depth_internal_types_probe)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    if(NOT TARGET Boost::headers AND NOT TARGET Boost::boost)
        find_package(Boost 1.90 CONFIG REQUIRED)
    endif()
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/src;${nijigenerate_common_sdk_root}/core")
    if(TARGET Boost::headers)
        set(CMAKE_REQUIRED_LIBRARIES Boost::headers)
    else()
        set(CMAKE_REQUIRED_LIBRARIES Boost::boost)
    endif()
    # Public native headers also include the canonical nicxlive compatibility
    # providers. Preserve the real consumer include path while checking that
    # this public header remains independently compilable.
    # This is a header compile check. The try-compile project does not inherit
    # ordinary build-tree aliases such as nicxlive::common, and linking is not
    # needed to detect whether the public header exposes these private types.
    if(DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE)
        set(_nijigenerate_saved_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
        set(_nijigenerate_had_try_compile_target_type TRUE)
    else()
        set(_nijigenerate_had_try_compile_target_type FALSE)
    endif()
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    unset(NIJIGENERATE_INTERNAL_PUBLIC_HEADER_VALID CACHE)
    check_cxx_source_compiles("#include <nijigenerate/compat/retained_string.hpp>\n#include <nijigenerate/io/depthimage.hpp>\nint main(){ nijigenerate::io::depthimage::DepthDrawSplitLayer value; }"
        NIJIGENERATE_INTERNAL_PUBLIC_HEADER_VALID)
    if(NOT NIJIGENERATE_INTERNAL_PUBLIC_HEADER_VALID)
        message(FATAL_ERROR "Internal type privacy gate requires a compiling public header")
    endif()
    foreach(_private_type DepthDrawConvolutionSample TupleIndexValue DepthDrawHistogramIntegral DepthDrawHistogramStats
            DepthDrawMaskedDepthEstimate DepthDrawMaskedSparseGridDepths)
        unset(NIJIGENERATE_PUBLIC_${_private_type} CACHE)
        check_cxx_source_compiles("#include <nijigenerate/compat/retained_string.hpp>\n#include <nijigenerate/io/depthimage.hpp>\nint main(){ nijigenerate::io::depthimage::detail::${_private_type}* value=nullptr; }"
            NIJIGENERATE_PUBLIC_${_private_type})
        if(NIJIGENERATE_PUBLIC_${_private_type})
            message(FATAL_ERROR "Private image type leaked through public API: ${_private_type}")
        endif()
    endforeach()
    if(_nijigenerate_had_try_compile_target_type)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_nijigenerate_saved_try_compile_target_type}")
    else()
        unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
    endif()
    unset(_nijigenerate_saved_try_compile_target_type)
    unset(_nijigenerate_had_try_compile_target_type)
    cmake_pop_check_state()
endif()
