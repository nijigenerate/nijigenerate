if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/base.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET binding_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/base.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/drag_data.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/point_state.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/clipboard_parameter_state.hpp")
    if(NOT TARGET nijigenerate_binding_base)
        add_library(nijigenerate_binding_base INTERFACE)
        target_include_directories(nijigenerate_binding_base INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_binding_base INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_binding_base INTERFACE nijigenerate_runtime_core)
    endif()
endif()
