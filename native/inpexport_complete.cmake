if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_gl_backend_source)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inpexport.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inpexport.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/typed_format_texture.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_gl_backend_source)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inpexport.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/inpexport.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
