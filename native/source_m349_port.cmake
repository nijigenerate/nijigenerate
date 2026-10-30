if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_autorig_contracts
    AND TARGET nijigenerate_autorig_neck AND TARGET nijigenerate_autorig_anatomy
    AND TARGET nijigenerate_phobos_stdio AND NOT TARGET nijigenerate_source_m349)
    add_library(nijigenerate_source_m349 STATIC
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_head_neck.cpp")
    target_include_directories(nijigenerate_source_m349 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tests/source")
    target_sources(nijigenerate_source_m349 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/source"
        FILES "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_head_neck.hpp")
    target_compile_features(nijigenerate_source_m349 PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_source_m349 PUBLIC
        nijigenerate_runtime_core nijigenerate_autorig_contracts
        nijigenerate_autorig_neck nijigenerate_autorig_anatomy nijigenerate_phobos_stdio)
    if(MSVC)
        target_compile_options(nijigenerate_source_m349 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m349 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    add_executable(nijigenerate_source_autorig_head_neck EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/autorig_head_neck_main.cpp")
    target_link_libraries(nijigenerate_source_autorig_head_neck PRIVATE nijigenerate_source_m349)
    if(MINGW)
        target_link_options(nijigenerate_source_autorig_head_neck PRIVATE -municode)
    endif()
endif()
