# The original resource module includes real SDK proxies as well as its base.
add_library(nijigenerate_selector_resource INTERFACE)
target_link_libraries(nijigenerate_selector_resource INTERFACE nijigenerate_core)
if(TARGET nicxlive::common)
    target_link_libraries(nijigenerate_selector_resource INTERFACE nicxlive::common)
endif()
target_sources(nijigenerate_selector_resource INTERFACE
    FILE_SET selector_resource_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/resource.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/resource_proxy.hpp")
if(TARGET nicxlive::nicxlive)
    target_link_libraries(nijigenerate_selector_resource INTERFACE nicxlive::nicxlive)
endif()
if(TARGET nijigenerate_runtime_core)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_selector_resource)
endif()
