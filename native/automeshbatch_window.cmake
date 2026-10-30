if(TARGET nijigenerate_vertex_viewport AND TARGET nijigenerate_tasks AND
        TARGET nijigenerate_modal AND TARGET nijigenerate_crashdump AND
        NOT TARGET nijigenerate_automeshbatch_window)
    add_library(nijigenerate_automeshbatch_window STATIC
        src/nijigenerate/windows/automeshbatch.cpp)
    target_include_directories(nijigenerate_automeshbatch_window PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_automeshbatch_window PUBLIC
        FILE_SET automeshbatch_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/automeshbatch.hpp")
    target_compile_features(nijigenerate_automeshbatch_window PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_automeshbatch_window PUBLIC
        nijigenerate_vertex_viewport nijigenerate_tasks nijigenerate_modal
        nijigenerate_inputtext nijigenerate_runtime_core
        PRIVATE nijigenerate_crashdump nijigenerate_button nijigenerate_dummy
        nijigenerate_label nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_automeshbatch_window PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_automeshbatch_window PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
