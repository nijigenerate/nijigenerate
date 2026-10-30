# Include after the runtime, binding actions and mutation publisher registrations.
if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    # GroupAction, binding actions and the NullableString formatter live in
    # runtime_core. Publish this actual dependent module from the same target.
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/drawable.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET drawable_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/drawable.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nicxlive::nicxlive)
endif()
