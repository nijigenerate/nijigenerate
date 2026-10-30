if(TARGET nijigenerate_autorig_registered AND TARGET nijigenerate_autorig_neck AND NOT TARGET nijigenerate_autorig_evidence)
    add_library(nijigenerate_autorig_evidence STATIC
        src/nijigenerate/autorig/deterministic/evidence_names.cpp
        src/nijigenerate/autorig/deterministic/evidence_geometry.cpp
        src/nijigenerate/autorig/deterministic/evidence_classify.cpp
        src/nijigenerate/autorig/deterministic/evidence_derive.cpp)
    target_compile_features(nijigenerate_autorig_evidence PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_evidence PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/evidence.hpp")
    target_link_libraries(nijigenerate_autorig_evidence PUBLIC nijigenerate_autorig_registered nijigenerate_autorig_neck)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_evidence PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_evidence PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
