if(TARGET nijigenerate_autorig_program AND TARGET nijigenerate_autorig_surface
        AND TARGET nijigenerate_autorig_face AND TARGET nijigenerate_autorig_review
        AND TARGET nijigenerate_autorig_storage AND TARGET nijigenerate_autorig_pipeline)
    add_library(nijigenerate_autorig_processor STATIC src/nijigenerate/autorig/deterministic/processor.cpp)
    target_sources(nijigenerate_autorig_processor PRIVATE
        src/nijigenerate/autorig/patterns/core.cpp src/nijigenerate/autorig/patterns/processor.cpp
        src/nijigenerate/autorig/patterns/detection.cpp src/nijigenerate/autorig/patterns/planning.cpp)
    target_sources(nijigenerate_autorig_processor PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES src/nijigenerate/autorig/deterministic/processor.hpp
            src/nijigenerate/autorig/patterns/core.hpp src/nijigenerate/autorig/patterns/processor.hpp
            src/nijigenerate/autorig/patterns/detection.hpp)
    target_compile_features(nijigenerate_autorig_processor PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_processor PUBLIC
        nijigenerate_autorig_program nijigenerate_autorig_surface nijigenerate_autorig_face
        nijigenerate_autorig_review nijigenerate_autorig_storage nijigenerate_autorig_pipeline
        nijigenerate_autorig_framework Boost::json)
    add_executable(nijigenerate_autorig_patterns_batch EXCLUDE_FROM_ALL tools/autorig_patterns_batch.cpp)
    target_link_libraries(nijigenerate_autorig_patterns_batch PRIVATE nijigenerate_autorig_processor)
    add_executable(nijigenerate_autorig_patterns_tests EXCLUDE_FROM_ALL
        tests/source/autorig_patterns_test.cpp src/nijigenerate/autorig/patterns/core.cpp
        src/nijigenerate/autorig/patterns/detection.cpp src/nijigenerate/autorig/patterns/planning.cpp)
    target_compile_features(nijigenerate_autorig_patterns_tests PRIVATE cxx_std_20)
    target_include_directories(nijigenerate_autorig_patterns_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_autorig_patterns_tests PRIVATE Boost::json)
    add_executable(nijigenerate_autorig_pattern_detection_tests EXCLUDE_FROM_ALL
        tests/source/autorig_pattern_detection_test.cpp src/nijigenerate/autorig/patterns/core.cpp
        src/nijigenerate/autorig/patterns/detection.cpp src/nijigenerate/autorig/patterns/planning.cpp)
    target_compile_features(nijigenerate_autorig_pattern_detection_tests PRIVATE cxx_std_20)
    target_include_directories(nijigenerate_autorig_pattern_detection_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_autorig_pattern_detection_tests PRIVATE Boost::json)
    if(BUILD_TESTING)
        add_test(NAME nijigenerate_autorig_patterns COMMAND nijigenerate_autorig_patterns_tests)
        add_test(NAME nijigenerate_autorig_pattern_detection COMMAND nijigenerate_autorig_pattern_detection_tests)
    endif()
    add_executable(nijigenerate_autorig_pattern_workflow_tests EXCLUDE_FROM_ALL
        tests/source/autorig_pattern_workflow_test.cpp)
    target_compile_features(nijigenerate_autorig_pattern_workflow_tests PRIVATE cxx_std_20)
    target_link_libraries(nijigenerate_autorig_pattern_workflow_tests PRIVATE
        nijigenerate_autorig_processor nijigenerate_autorig_workflow)
    if(BUILD_TESTING)
        add_test(NAME nijigenerate_autorig_pattern_workflow COMMAND nijigenerate_autorig_pattern_workflow_tests)
    endif()
    if(MSVC)
        target_compile_options(nijigenerate_autorig_processor PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_processor PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
