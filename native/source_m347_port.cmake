# Original tests/autorig_alpha_support.d, using a caller-supplied real cloud JSON.
if(TARGET nijigenerate_autorig_evidence AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_phobos_stdio AND TARGET SDL3::SDL3
        AND NOT TARGET nijigenerate_source_autorig_alpha_support)
    add_executable(nijigenerate_source_autorig_alpha_support EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_alpha_support.cpp")
    target_compile_features(nijigenerate_source_autorig_alpha_support PRIVATE cxx_std_20)
    target_link_libraries(nijigenerate_source_autorig_alpha_support PRIVATE
        nijigenerate_autorig_evidence nijigenerate_runtime_core nijigenerate_phobos_stdio SDL3::SDL3)
    if(MSVC)
        target_compile_options(nijigenerate_source_autorig_alpha_support PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_autorig_alpha_support PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
