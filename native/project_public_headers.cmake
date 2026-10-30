if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_io
        AND TARGET nijigenerate_gl_backend_source)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET project_public_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET project_signal_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/signal.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/signal.LICENSE")
    # The source publicly imports the actual IO and atlas APIs. Atlas's actual
    # implementation is in gl_backend_source; no surrogate export target.
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_io nijigenerate_gl_backend_source)
endif()
