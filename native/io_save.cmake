if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_io
        AND TARGET nijigenerate_window_base AND TARGET nijigenerate_imagefmt_writer
        AND NOT TARGET nijigenerate_io_save)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_save.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/project_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/puppet_serialize.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET project_writer_source_api TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/project_writer.hpp")
    target_include_directories(nijigenerate_runtime_core PRIVATE "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_autosave
        nijigenerate_task_notification_status nijigenerate_dialog_queue nijigenerate_imagefmt_writer)
    add_library(nijigenerate_io_save STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/save.cpp")
    target_sources(nijigenerate_io_save PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/save.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/save_settings.hpp")
    target_compile_features(nijigenerate_io_save PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_io_save PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_io_save PUBLIC nijigenerate_runtime_core nijigenerate_io
        nijigenerate_window_base nijigenerate_dialog_queue)
    # Welcome and project open/save form the same real static-library dependency group.
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_io_save)
endif()
