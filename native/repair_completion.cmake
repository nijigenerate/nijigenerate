if(TARGET nijigenerate_repair_ids)
    target_compile_features(nijigenerate_repair_ids PUBLIC cxx_std_20)
    target_sources(nijigenerate_repair_ids PUBLIC FILE_SET repair_texture_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/texture_pixels.hpp")
    target_sources(nijigenerate_repair_ids PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/utils/repair.cpp")
    if(TARGET nijigenerate_runtime_core)
        # Real puppet tool commands call all three repair entry points.
        target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_repair_ids)
    endif()
endif()
