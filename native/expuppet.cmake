if(BUILD_TESTING AND TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    add_library(nijigenerate_expuppet_declaration_probe OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_declaration_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_declaration_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_declaration_probe PRIVATE cxx_std_20)
    add_executable(nijigenerate_expuppet_identity_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_identity_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_identity_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_identity_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_expuppet_declaration_probe PRIVATE /Z7)
        target_compile_options(nijigenerate_expuppet_identity_probe PRIVATE /Z7)
    endif()
    if(WIN32)
        add_custom_command(TARGET nijigenerate_expuppet_identity_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_expuppet_identity_probe>
                $<TARGET_FILE_DIR:nijigenerate_expuppet_identity_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_custom_target(nijigenerate_expuppet_project)
    add_executable(nijigenerate_expuppet_fields_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_fields_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_fields_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_fields_probe PRIVATE cxx_std_20)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_expuppet_fields_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_expuppet_fields_probe>
                $<TARGET_FILE_DIR:nijigenerate_expuppet_fields_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_dependencies(nijigenerate_expuppet_project
        nijigenerate_expuppet_declaration_probe nijigenerate_expuppet_identity_probe
        nijigenerate_expuppet_fields_probe)
    add_executable(nijigenerate_expuppet_constructors_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_constructors_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_constructors_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_constructors_probe PRIVATE cxx_std_20)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_expuppet_constructors_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_expuppet_constructors_probe>
                $<TARGET_FILE_DIR:nijigenerate_expuppet_constructors_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_dependencies(nijigenerate_expuppet_project nijigenerate_expuppet_constructors_probe)
    add_executable(nijigenerate_expuppet_queries_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_queries_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_queries_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_queries_probe PRIVATE cxx_std_20)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_expuppet_queries_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_expuppet_queries_probe>
                $<TARGET_FILE_DIR:nijigenerate_expuppet_queries_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_dependencies(nijigenerate_expuppet_project nijigenerate_expuppet_queries_probe)
    add_executable(nijigenerate_expuppet_mutations_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/expuppet_mutations_probe.cpp")
    target_link_libraries(nijigenerate_expuppet_mutations_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_expuppet_mutations_probe PRIVATE cxx_std_20)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_expuppet_mutations_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_expuppet_mutations_probe>
                $<TARGET_FILE_DIR:nijigenerate_expuppet_mutations_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_dependencies(nijigenerate_expuppet_project nijigenerate_expuppet_mutations_probe)
    add_test(NAME native_expuppet_declaration_identity COMMAND nijigenerate_expuppet_identity_probe)
    add_test(NAME native_expuppet_constructors COMMAND nijigenerate_expuppet_constructors_probe)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_expuppet_group_names_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_expuppet_queries.py"
                --source-root "${PROJECT_SOURCE_DIR}" --kind groups
                --native "$<TARGET_FILE:nijigenerate_expuppet_queries_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/expuppet-group-names-evidence/$<CONFIG>")
        add_test(NAME native_expuppet_mutations_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_expuppet_mutations.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_expuppet_mutations_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/expuppet-mutations-evidence/$<CONFIG>")
        add_test(NAME native_expuppet_queries_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_expuppet_queries.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_expuppet_queries_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/expuppet-queries-evidence/$<CONFIG>")
        add_test(NAME native_expuppet_fields_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_expuppet_fields.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_expuppet_fields_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/expuppet-fields-evidence/$<CONFIG>")
    endif()
endif()
