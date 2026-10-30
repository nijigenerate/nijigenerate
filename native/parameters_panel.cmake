if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_binding.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_buttons.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_view.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_panel.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET parameters_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_panel.hpp")
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_view.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_panel.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_view.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/parameters_panel.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
