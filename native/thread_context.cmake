target_sources(nijigenerate_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/thread_context.cpp")
find_package(Threads REQUIRED)
target_link_libraries(nijigenerate_core PUBLIC Threads::Threads)
target_sources(nijigenerate_core PUBLIC FILE_SET thread_context_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/thread_context.hpp")
