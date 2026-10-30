option(NIJIGENERATE_TRANSLATION_CATALOGS "Compile and stage current translation catalogs" OFF)

function(ng_stage_translation_catalogs target)
    if(NOT TARGET nijigenerate_translation_catalogs)
        message(FATAL_ERROR "Enable NIJIGENERATE_TRANSLATION_CATALOGS before staging catalogs")
    endif()
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown translation catalog consumer: ${target}")
    endif()
    get_target_property(is_bundle "${target}" MACOSX_BUNDLE)
    if(APPLE AND is_bundle)
        set(destination "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/i18n")
    elseif(WIN32)
        set(destination "$<TARGET_FILE_DIR:${target}>/i18n")
    else()
        set(destination "$<TARGET_FILE_DIR:${target}>")
    endif()
    add_custom_target("${target}_catalogs" ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${NG_TRANSLATION_CATALOG_FILES} "${destination}"
        DEPENDS "${target}" nijigenerate_translation_catalogs
        VERBATIM)
endfunction()

if(NIJIGENERATE_TRANSLATION_CATALOGS)
    find_program(NIJIGENERATE_MSGFMT NAMES msgfmt REQUIRED)
    file(GLOB translation_inputs CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tl/*.po")
    list(SORT translation_inputs)
    set(NG_TRANSLATION_CATALOG_FILES)
    set(translation_output "${CMAKE_CURRENT_BINARY_DIR}/catalogs")
    foreach(translation_input IN LISTS translation_inputs)
        get_filename_component(translation_code "${translation_input}" NAME_WE)
        set(catalog "${translation_output}/${translation_code}.mo")
        add_custom_command(OUTPUT "${catalog}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${translation_output}"
            COMMAND "${NIJIGENERATE_MSGFMT}" -o "${catalog}" -- "${translation_input}"
            DEPENDS "${translation_input}" "${NIJIGENERATE_MSGFMT}"
            VERBATIM)
        list(APPEND NG_TRANSLATION_CATALOG_FILES "${catalog}")
    endforeach()
    if(NOT NG_TRANSLATION_CATALOG_FILES)
        message(FATAL_ERROR "No current PO catalogs found")
    endif()
    add_custom_target(nijigenerate_translation_catalogs ALL DEPENDS ${NG_TRANSLATION_CATALOG_FILES})
    if(WIN32)
        set(catalog_install_destination "bin/i18n")
    elseif(APPLE)
        set(catalog_install_destination "Resources/i18n")
    else()
        set(catalog_install_destination "bin")
    endif()
    install(FILES ${NG_TRANSLATION_CATALOG_FILES} DESTINATION "${catalog_install_destination}" COMPONENT Translations)
    if(BUILD_TESTING)
        add_executable(nijigenerate_catalog_layout_probe "${CMAKE_CURRENT_LIST_DIR}/tests/catalog_layout_probe.cpp")
        target_link_libraries(nijigenerate_catalog_layout_probe PRIVATE nijigenerate_core)
        ng_stage_translation_catalogs(nijigenerate_catalog_layout_probe)
        if(NIJIGENERATE_PYTHON)
            if(WIN32)
                set(catalog_test_directory "$<TARGET_FILE_DIR:nijigenerate_catalog_layout_probe>/i18n")
            else()
                set(catalog_test_directory "$<TARGET_FILE_DIR:nijigenerate_catalog_layout_probe>")
            endif()
            add_test(NAME native_translation_catalog_layout
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/check_catalog_layout.py"
                    --native "$<TARGET_FILE:nijigenerate_catalog_layout_probe>"
                    --catalog-root "${catalog_test_directory}" --po-root "${PROJECT_SOURCE_DIR}/tl"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/catalog-evidence/$<CONFIG>/result.json")
        endif()
    endif()
endif()
