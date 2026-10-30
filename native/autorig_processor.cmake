if(TARGET nijigenerate_autorig_program AND TARGET nijigenerate_autorig_surface
        AND TARGET nijigenerate_autorig_face AND TARGET nijigenerate_autorig_review
        AND TARGET nijigenerate_autorig_storage AND TARGET nijigenerate_autorig_pipeline)
    add_library(nijigenerate_autorig_processor STATIC src/nijigenerate/autorig/deterministic/processor.cpp)
    target_sources(nijigenerate_autorig_processor PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES src/nijigenerate/autorig/deterministic/processor.hpp)
    target_compile_features(nijigenerate_autorig_processor PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_autorig_processor PUBLIC
        nijigenerate_autorig_program nijigenerate_autorig_surface nijigenerate_autorig_face
        nijigenerate_autorig_review nijigenerate_autorig_storage nijigenerate_autorig_pipeline
        nijigenerate_autorig_framework)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_processor PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_processor PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
