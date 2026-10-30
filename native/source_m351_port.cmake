if(TARGET nijigenerate_autorig_neck AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_phobos_stdio AND TARGET SDL3::SDL3
        AND NOT TARGET nijigenerate_source_autorig_neck)
    add_executable(nijigenerate_source_autorig_neck EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_neck.cpp")
    target_compile_features(nijigenerate_source_autorig_neck PRIVATE cxx_std_20)
    target_link_libraries(nijigenerate_source_autorig_neck PRIVATE
        nijigenerate_autorig_neck nijigenerate_runtime_core nijigenerate_phobos_stdio SDL3::SDL3)
    if(MSVC)
        target_compile_options(nijigenerate_source_autorig_neck PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_autorig_neck PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
