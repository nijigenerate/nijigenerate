# Full-file PSD dependency foundation; RetainedFile is provided by nijigenerate_core.
if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/io.cpp")
    target_sources(nijigenerate_core PUBLIC
        FILE_SET psd_io_provider_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/io.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/utils.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psd/retained_file.hpp")
endif()
