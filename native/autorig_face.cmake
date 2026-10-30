if(TARGET nijigenerate_quadratic)
    add_library(nijigenerate_autorig_face STATIC src/nijigenerate/autorig/deterministic/face.cpp)
    target_compile_features(nijigenerate_autorig_face PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_face PUBLIC FILE_SET autorig_face_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/face.hpp")
    target_link_libraries(nijigenerate_autorig_face PUBLIC nijigenerate_quadratic)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_face PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_face PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
