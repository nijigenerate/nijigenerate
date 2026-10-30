if(TARGET nijigenerate_autorig_framework)
    add_library(nijigenerate_autorig_workflow STATIC
        src/nijigenerate/autorig/workflow.cpp
        src/nijigenerate/autorig/workflow_processor.cpp
        src/nijigenerate/autorig/workflow_run.cpp
        src/nijigenerate/autorig/workflow_manager.cpp)
    target_compile_features(nijigenerate_autorig_workflow PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_workflow PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/workflow.hpp")
    target_link_libraries(nijigenerate_autorig_workflow PUBLIC nijigenerate_autorig_framework)
endif()
