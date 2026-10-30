if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base AND TARGET nijigenerate_resources)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET welcome_window_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/welcome.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/welcome.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_window_base nijigenerate_resources nijigenerate_io
        nijigenerate_label nijigenerate_button nijigenerate_dummy nijigenerate_shadow
        nijigenerate_dpi)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/welcome.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/welcome.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
