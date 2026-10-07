cmake_minimum_required(VERSION 3.24)
project(nijigenerate_osqp LANGUAGES C)

get_filename_component(NG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
foreach(component osqp qdldl)
    if(NOT EXISTS "${NG_ROOT}/vendor/${component}/CMakeLists.txt")
        message(FATAL_ERROR "Missing vendor/${component}; see doc/osqp-integration.md")
    endif()
endforeach()

# Use the pinned local QDLDL checkout rather than OSQP's network fetch.
set(FETCHCONTENT_SOURCE_DIR_QDLDL "${NG_ROOT}/vendor/qdldl" CACHE PATH "" FORCE)
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
set(OSQP_ALGEBRA_BACKEND builtin CACHE STRING "" FORCE)
set(OSQP_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(OSQP_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(OSQP_BUILD_DEMO_EXE OFF CACHE BOOL "" FORCE)
set(OSQP_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
set(OSQP_USE_FLOAT OFF CACHE BOOL "" FORCE)
set(OSQP_USE_LONG OFF CACHE BOOL "" FORCE)
set(OSQP_ENABLE_PRINTING OFF CACHE BOOL "" FORCE)
set(OSQP_ENABLE_INTERRUPT OFF CACHE BOOL "" FORCE)
set(OSQP_ENABLE_PROFILING ON CACHE BOOL "" FORCE)
set(OSQP_CODEGEN OFF CACHE BOOL "" FORCE)
set(OSQP_ENABLE_DERIVATIVES OFF CACHE BOOL "" FORCE)
add_subdirectory("${NG_ROOT}/vendor/osqp" "${CMAKE_CURRENT_BINARY_DIR}/osqp")
add_library(nijigenerate_osqp STATIC bridge.c)
target_link_libraries(nijigenerate_osqp PUBLIC osqpstatic)
install(TARGETS nijigenerate_osqp ARCHIVE DESTINATION lib)

# Keep all upstream license files, including bundled AMD notices.
foreach(component osqp qdldl)
    install(DIRECTORY "${NG_ROOT}/vendor/${component}/"
        DESTINATION "share/licenses/${component}"
        FILES_MATCHING PATTERN "LICENSE*" PATTERN "NOTICE*"
        PATTERN ".git" EXCLUDE)
endforeach()
