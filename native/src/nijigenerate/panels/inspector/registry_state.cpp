#include "detail/registry_state.hpp"

namespace nijigenerate::panels::inspector::detail {

thread_local NodeInspectorFactories nodeInspectors;
thread_local bool inspectorsInitialized = false;

} // namespace nijigenerate::panels::inspector::detail
