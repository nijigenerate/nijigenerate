option(NIJIGENERATE_DEBUG_AUTOMESH_OPT "Map named D debug(automesh_opt) source diagnostics" OFF)
option(NIJIGENERATE_DEBUG_AUTOMESH_OPT_FULL "Map named D debug(automesh_opt_full) source diagnostics" OFF)
option(NIJIGENERATE_NO_AUTODECODE_STRINGS "Map the Phobos NoAutodecodeStrings source version" OFF)

if(TARGET nijigenerate_automesh_processor)
    target_sources(nijigenerate_automesh_processor PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_fuse_shape.cpp")
    if(TARGET nijigenerate_core)
        target_link_libraries(nijigenerate_automesh_processor PUBLIC nijigenerate_core)
    endif()
    # These public templates are instantiated in their consumers' translation units.
    if(MSVC)
        target_compile_options(nijigenerate_automesh_processor PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_processor PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
    if(NIJIGENERATE_NO_AUTODECODE_STRINGS)
        target_compile_definitions(nijigenerate_automesh_processor PUBLIC NG_NO_AUTODECODE_STRINGS)
    endif()
    target_sources(nijigenerate_automesh_processor PUBLIC
        FILE_SET automesh_optimum_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/mir_any.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/byte_view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_byte_view_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_const_slice_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/contiguous_view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_slice_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_image_slice_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_cv_slice_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_ranges.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_bool.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_callable.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_reference.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_call_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_delegate.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_mir_predicate.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_unicode_cursor.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_unicode_exception.hpp")
    if(BUILD_TESTING)
        add_executable(nijigenerate_range_bool_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/range_bool_probe.cpp")
        target_link_libraries(nijigenerate_range_bool_probe PRIVATE nijigenerate_automesh_processor)
        add_test(NAME native_range_bool_storage COMMAND nijigenerate_range_bool_probe)
    endif()
endif()

if(TARGET nijigenerate_automesh_processor AND TARGET nijigenerate_automesh_common
        AND TARGET nijigenerate_automesh_contours AND TARGET nijigenerate_cv
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_category
        AND TARGET nijigenerate_drag AND TARGET nijigenerate_label
        AND TARGET nijigenerate_dummy AND TARGET nijigenerate_button)
    add_library(nijigenerate_automesh_optimum STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum_geometry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum_image.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum_coverage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum_presets.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/optimum_reflection.cpp")
    target_include_directories(nijigenerate_automesh_optimum PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_optimum PUBLIC cxx_std_20)
    target_sources(nijigenerate_automesh_optimum PUBLIC
        FILE_SET automesh_source_image_slice_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_image_slice_view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_cv_slice_view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_delegate.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_mir_predicate.hpp")
    if(NIJIGENERATE_DEBUG_AUTOMESH_OPT)
        target_compile_definitions(nijigenerate_automesh_optimum PRIVATE NG_DEBUG_AUTOMESH_OPT)
    endif()
    if(NIJIGENERATE_DEBUG_AUTOMESH_OPT_FULL)
        target_compile_definitions(nijigenerate_automesh_optimum PRIVATE NG_DEBUG_AUTOMESH_OPT_FULL)
    endif()
    target_link_libraries(nijigenerate_automesh_optimum PUBLIC
        nijigenerate_automesh_processor nijigenerate_automesh_common nijigenerate_automesh_contours
        nijigenerate_cv nijigenerate_runtime_core nijigenerate_core nijigenerate_category
        nijigenerate_drag nijigenerate_label nijigenerate_dummy nijigenerate_button)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_optimum PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_optimum PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
