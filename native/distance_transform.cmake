# The independent numeric module is available without the stackful CV package.
# distance1d.cpp is already compiled exactly once by nijigenerate_core.
include_guard(GLOBAL)
if(NOT TARGET nijigenerate_core)
    message(FATAL_ERROR "distance_transform.cmake requires nijigenerate_core")
endif()

# The independent public template contains NaN/INF comparisons and float math.
# Its consumers need the same policy as the optional CV package, not only the TU.
if(MSVC)
    target_compile_options(nijigenerate_core PUBLIC /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nijigenerate_core PUBLIC -ffp-contract=off -fno-fast-math)
endif()

target_sources(nijigenerate_core PUBLIC
    FILE_SET distance_transform_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/distance1d.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/pixel_operations.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/distance_conversion.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/distancetransform.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/contiguous_view.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/native_access_fault.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/slice.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_context.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_array.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_element_definition.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_contour_value_lifetime.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_meta_value_lifetime.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_value_lifetime.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_integer_vector_lifetime.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_shared_scalar.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_plain_value.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_immutable_value.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_rc_qualified_definition.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/retained_string_slice.hpp")
