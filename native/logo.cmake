option(NIJIGENERATE_LOGO_RESOURCES "Build logo initialization with canonical textures and embedded assets" OFF)
if(TARGET nicxlive::nicxlive AND TARGET nijigenerate_resources)
    add_library(nijigenerate_logo STATIC src/nijigenerate/core/logo.cpp)
    target_sources(nijigenerate_logo PUBLIC FILE_SET logo_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/logo.hpp")
    target_compile_features(nijigenerate_logo PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_logo PUBLIC nijigenerate_core nicxlive::nicxlive nijigenerate_resources)
    add_custom_target(nijigenerate_logo_project DEPENDS nijigenerate_logo)
    if(BUILD_TESTING)
        add_executable(nijigenerate_logo_probe tests/logo_probe.cpp)
        target_link_libraries(nijigenerate_logo_probe PRIVATE nijigenerate_logo)
        add_dependencies(nijigenerate_logo_project nijigenerate_logo_probe)
        if(WIN32)
            get_target_property(logo_json_type Boost::json TYPE)
            if(logo_json_type STREQUAL "SHARED_LIBRARY")
                add_custom_command(TARGET nijigenerate_logo_probe POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "$<TARGET_FILE:Boost::json>" "$<TARGET_FILE_DIR:nijigenerate_logo_probe>")
            endif()
        endif()
        add_test(NAME native_logo_resources COMMAND nijigenerate_logo_probe)
        set(NIJIGENERATE_IMAGEFMT_SOURCE_DIR "" CACHE PATH "Existing independent D imagefmt source directory")
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_IMAGEFMT_SOURCE_DIR)
            add_test(NAME native_logo_pixels
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/logo_pixels.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --imagefmt "${NIJIGENERATE_IMAGEFMT_SOURCE_DIR}"
                    --native "$<TARGET_FILE:nijigenerate_logo_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/logo-pixels/$<CONFIG>")
        endif()
    endif()
elseif(NIJIGENERATE_LOGO_RESOURCES)
    message(FATAL_ERROR "Logo resources require the real nicxlive and embedded-resource providers")
endif()
