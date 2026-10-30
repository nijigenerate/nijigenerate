if(TARGET nijigenerate_runtime_core)
    # templates.cpp is already part of this actual runtime target.
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET autorig_template_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/templates.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/detail/embedded_templates.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/detail/sha256.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/detail/json_first_pass_reader.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/detail/json_numeric_first_pass.hpp")
endif()
