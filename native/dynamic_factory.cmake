if(TARGET nicxlive::nicxlive)
    add_library(nijigenerate_dynamic_factory STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/dynamic_factory.cpp")
    target_link_libraries(nijigenerate_dynamic_factory PUBLIC nijigenerate_core nicxlive::nicxlive)
endif()
