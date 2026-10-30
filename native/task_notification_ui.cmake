if(NIJIGENERATE_IMGUI)
    target_sources(nijigenerate_task_notification_status PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/task_hover_status.cpp")
    target_link_libraries(nijigenerate_task_notification_status PUBLIC nijigenerate_imgui_sdl3)
endif()
