# Include after depth_draw_session.cmake; the runtime target owns the actual node providers.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/autobind.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/autobind.hpp")
    target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
endif()
