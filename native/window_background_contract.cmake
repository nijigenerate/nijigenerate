if(BUILD_TESTING AND TARGET nicxlive::nicxlive)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_clear_color_thread_observation tests/clear_color_thread_observation.cpp)
    target_link_libraries(nijigenerate_clear_color_thread_observation PRIVATE
        nijigenerate_native_options nicxlive::nicxlive Threads::Threads)
    add_custom_target(nijigenerate_window_background_contract_project
        DEPENDS nijigenerate_clear_color_thread_observation)
    # Diagnostic executable only: a known domain difference is not a passing
    # normative regression test or acceptance of native frontend implementations.
endif()
