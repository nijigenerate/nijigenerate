# Call after selecting the actual importer_complete.cmake source registration.
function(ng_publish_psd_adapter)
    if(NOT TARGET nijigenerate_runtime_core OR NOT TARGET nijigenerate_io)
        message(FATAL_ERROR "PSD adapter requires actual runtime-core and I/O providers")
    endif()
    get_target_property(published nijigenerate_runtime_core NG_PSD_ADAPTER_PUBLISHED)
    if(NOT published)
        target_sources(nijigenerate_runtime_core PUBLIC FILE_SET psd_adapter_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/io/psd.hpp")
        target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
        set_property(TARGET nijigenerate_runtime_core PROPERTY NG_PSD_ADAPTER_PUBLISHED TRUE)
    endif()
endfunction()
