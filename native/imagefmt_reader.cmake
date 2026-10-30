if(TARGET nijigenerate_imagefmt_writer AND NOT TARGET nijigenerate_imagefmt_reader)
    add_library(nijigenerate_imagefmt_reader INTERFACE)
    target_include_directories(nijigenerate_imagefmt_reader INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_imagefmt_reader
        INTERFACE nijigenerate_imagefmt_writer)
endif()

option(NIJIGENERATE_IMAGEFMT_COMPONENT_CHECKS "Build imagefmt public API component checks" OFF)
if(NIJIGENERATE_IMAGEFMT_COMPONENT_CHECKS AND TARGET nijigenerate_imagefmt_reader)
    add_executable(nijigenerate_imagefmt_component_check
        "${CMAKE_CURRENT_LIST_DIR}/tests/imagefmt_component_check.cpp")
    target_link_libraries(nijigenerate_imagefmt_component_check PRIVATE nijigenerate_imagefmt_reader)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_imagefmt_native_observer
        "${CMAKE_CURRENT_LIST_DIR}/tests/imagefmt_native_observer.cpp")
    target_link_libraries(nijigenerate_imagefmt_native_observer PRIVATE
        nijigenerate_imagefmt_reader Threads::Threads)
    if(BUILD_TESTING)
        add_test(NAME native_imagefmt_component COMMAND nijigenerate_imagefmt_component_check)
    endif()
endif()
