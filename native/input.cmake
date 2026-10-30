if(NIJIGENERATE_IMGUI AND TARGET nijigenerate_dpi)
    add_library(nijigenerate_input STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input.cpp")
    target_sources(nijigenerate_input PUBLIC FILE_SET input_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_poll.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_io_state.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_mouse_position.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_mouse_state.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_events.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_key_up.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_key_scancode.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_shortcut_template.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_shortcut_spec.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_modifier_labels.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/input_format_shortcut.hpp")
    target_link_libraries(nijigenerate_input PUBLIC nijigenerate_dpi nijigenerate_imgui)
endif()
