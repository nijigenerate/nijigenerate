# Faithful source-module library only; no executable or new validation cases.
if(TARGET nijigenerate_autorig_processor AND TARGET nijigenerate_autorig_workflow
        AND TARGET nijigenerate_autorig_controls AND TARGET nijigenerate_autorig_observation
        AND TARGET nijigenerate_autorig_physics AND TARGET nijigenerate_autorig_presentation)
    add_library(nijigenerate_source_m352 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/src/autorig_pipeline.cpp")
    target_sources(nijigenerate_source_m352 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/autorig_pipeline.hpp")
    target_include_directories(nijigenerate_source_m352 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_source_m352 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m352 PUBLIC
        nijigenerate_autorig_processor nijigenerate_autorig_workflow
        nijigenerate_autorig_controls nijigenerate_autorig_observation
        nijigenerate_autorig_physics nijigenerate_autorig_presentation
        nijigenerate_autorig_contracts nijigenerate_autorig_evidence
        nijigenerate_autorig_program nijigenerate_autorig_framework nijigenerate_tasks)
    if(MSVC)
        target_compile_options(nijigenerate_source_m352 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m352 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
