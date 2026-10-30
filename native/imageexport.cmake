# Independent imagefmt 2.1.2 writer closure; no bundled stb placeholder is used.
if(NOT TARGET ZLIB::ZLIB)
    if(NOT DEFINED ZLIB_ROOT AND EXISTS "$ENV{USERPROFILE}/vcpkg/installed/x64-windows/include/zlib.h")
        set(ZLIB_ROOT "$ENV{USERPROFILE}/vcpkg/installed/x64-windows")
    endif()
    find_package(ZLIB QUIET)
endif()

if(TARGET ZLIB::ZLIB)
    include("${nijigenerate_common_sdk_root}/cmake/NicxliveImagefmt.cmake")
    add_library(nijigenerate_imagefmt_writer INTERFACE)
    target_include_directories(nijigenerate_imagefmt_writer INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_imagefmt_writer INTERFACE nijigenerate_native_options nicxlive::imagefmt)
endif()

# NotificationPopup's real virtual onUpdate body is enabled by the ImGui build.
if(TARGET nijigenerate_imagefmt_writer AND TARGET nijigenerate_task_notification_status AND NIJIGENERATE_IMGUI)
    add_library(nijigenerate_imageexport STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/imageexport.cpp")
    target_sources(nijigenerate_imageexport PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/imageexport.hpp")
    target_link_libraries(nijigenerate_imageexport PUBLIC nijigenerate_core
        PRIVATE nijigenerate_imagefmt_writer nijigenerate_task_notification_status)
else()
    message(STATUS "Image export requires real zlib and the ImGui notification provider")
endif()
