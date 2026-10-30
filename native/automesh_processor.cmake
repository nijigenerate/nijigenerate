if(TARGET nicxlive::nicxlive)
    add_library(nijigenerate_automesh_processor STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/automesh.cpp")
    target_include_directories(nijigenerate_automesh_processor PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_automesh_processor PUBLIC
        FILE_SET automesh_processor_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/automesh.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/shared_delegate.hpp")
    target_compile_features(nijigenerate_automesh_processor PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automesh_processor PUBLIC nicxlive::nicxlive)
endif()
