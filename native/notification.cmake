add_library(nijigenerate_notification_declarations INTERFACE)
target_include_directories(nijigenerate_notification_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_sources(nijigenerate_notification_declarations INTERFACE
    FILE_SET notification_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/notification.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/notification_item.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/compat/source_c_string.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/compat/shared_delegate.hpp")
target_link_libraries(nijigenerate_notification_declarations INTERFACE nijigenerate_native_options)
add_library(nijigenerate_notification STATIC src/nijigenerate/widgets/notification.cpp)
target_link_libraries(nijigenerate_notification PUBLIC nijigenerate_notification_declarations
    PRIVATE nicxlive::common)
if(NIJIGENERATE_IMGUI)
    target_sources(nijigenerate_notification PRIVATE src/nijigenerate/widgets/notification_update.cpp)
    target_link_libraries(nijigenerate_notification PRIVATE nijigenerate_imgui nijigenerate_button)
endif()
add_library(nijigenerate_task_notification_status STATIC src/nijigenerate/core/task_notification_status.cpp)
target_link_libraries(nijigenerate_task_notification_status PUBLIC nijigenerate_notification)
target_sources(nijigenerate_task_notification_status PUBLIC
    FILE_SET task_status_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/tasks.hpp")
include("${CMAKE_CURRENT_LIST_DIR}/task_notification_ui.cmake")
if(BUILD_TESTING)
    add_executable(nijigenerate_notification_item_probe tests/notification_item_probe.cpp)
    target_link_libraries(nijigenerate_notification_item_probe PRIVATE nijigenerate_notification_declarations)
    add_test(NAME native_notification_item_contract COMMAND nijigenerate_notification_item_probe)
    add_library(nijigenerate_notification_header_probe OBJECT tests/notification_header_probe.cpp)
    target_link_libraries(nijigenerate_notification_header_probe PRIVATE nijigenerate_notification_declarations)
endif()
