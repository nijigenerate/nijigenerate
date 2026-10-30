# Contour scanning suspends the actual stackful task, including header templates.
if(NIJIGENERATE_STACKFUL_TASKS AND NOT TARGET nijigenerate_cv)
    if(NOT TARGET nijigenerate_tasks)
        message(FATAL_ERROR "cv_package.cmake requires tasks.cmake before it")
    endif()
    add_library(nijigenerate_cv STATIC src/nijigenerate/core/cv/contours.cpp)
    target_sources(nijigenerate_cv PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/detail/contours_sort.hpp")
    target_link_libraries(nijigenerate_cv PUBLIC nijigenerate_core nijigenerate_tasks)
    target_compile_features(nijigenerate_cv PUBLIC cxx_std_20)
    target_sources(nijigenerate_cv PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/contours.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/pixel_operations.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/cv/image.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/const_slice.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_const_slice_identity.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/byte_view.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_reference.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/contiguous_view.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_cv PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_cv PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
elseif(NOT NIJIGENERATE_STACKFUL_TASKS)
    message(STATUS "nijigenerate_cv is disabled: enable NIJIGENERATE_STACKFUL_TASKS for the complete CV package")
endif()
