target_sources(nijigenerate_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_value_action.hpp")
# actions/node.cpp is already an existing canonical production source. Do not
# add another replacement body or duplicate provider in this header registration.
