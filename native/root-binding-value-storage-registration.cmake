get_filename_component(_binding_storage_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
target_sources(nijigenerate_core PRIVATE
    "${_binding_storage_root}/native/src/nijigenerate/compat/native_access_fault.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_storage.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_all_value_action.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_notification.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_interpolation_action.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_add_remove_action.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_action.hpp"
    "${_binding_storage_root}/native/src/nijigenerate/actions/binding_actions.hpp")
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET binding_action_headers TYPE HEADERS
        BASE_DIRS "${_binding_storage_root}/native/src"
        FILES
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_actions.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_storage.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_all_value_action.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_notification.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_interpolation_action.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_add_remove_action.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/actions/binding_value_action.hpp"
            "${_binding_storage_root}/native/src/nijigenerate/compat/native_access_fault.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${_binding_storage_root}/native/src/nijigenerate/actions/binding_actions.cpp")
elseif(NIJIGENERATE_RUNTIME_PROBE)
    target_sources(nijigenerate_core PRIVATE
        "${_binding_storage_root}/native/src/nijigenerate/actions/binding_actions.cpp")
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    target_link_libraries(nijigenerate_core PUBLIC nicxlive::nicxlive)
endif()
if(BUILD_TESTING AND NIJIGENERATE_RUNTIME_PROBE
        AND NOT TARGET nijigenerate_binding_value_storage_probe)
    add_executable(nijigenerate_binding_value_storage_probe
        "${_binding_storage_root}/native/tests/binding_value_storage_probe.cpp")
    target_link_libraries(nijigenerate_binding_value_storage_probe PRIVATE
        nijigenerate_core nicxlive::nicxlive)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_binding_value_storage_probe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_binding_value_storage_probe>
                $<TARGET_FILE_DIR:nijigenerate_binding_value_storage_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_binding_value_storage COMMAND nijigenerate_binding_value_storage_probe)
    set_tests_properties(native_binding_value_storage PROPERTIES TIMEOUT 30)
endif()
