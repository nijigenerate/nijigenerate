#include "apply_node_actions.hpp"
#include <nijigenerate/panels/inspector/pathdeform.hpp>

namespace nijigenerate::commands::inspector::apply_node {

using Path = panels::inspector::pathdeform::PathDeformer;
using PathInspector = panels::inspector::pathdeform::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, Path>;
using PathState = panels::inspector::pathdeform::detail::NodeInspectorState<
    panels::inspector::ModelEditSubMode::Layout, Path>;

// Instantiate the real PathDeformer property/action bodies required by M179.
// Context dispatch and actual Layout UI remain their separate unfinished bodies.
template class InspectorAttributeAction<PathInspector, InspectorAttributeAccess<PathInspector, &PathState::dynamic>>;
template class InspectorAttributeAction<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::physicsEnabled>>;
template class InspectorAttributeAction<PathInspector, InspectorAttributeAccess<PathInspector, &PathState::curveType>>;
template class InspectorAttributeAction<PathInspector, InspectorAttributeAccess<PathInspector, &PathState::gravity>>;
template class InspectorAttributeAction<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::restoreConstant>>;
template class InspectorAttributeAction<PathInspector, InspectorAttributeAccess<PathInspector, &PathState::damping>>;
template class InspectorAttributeAction<PathInspector, InspectorAttributeAccess<PathInspector, &PathState::inputScale>>;
template class InspectorAttributeAction<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::propagateScale>>;
template class InspectorToggleAttributeAction<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::dynamic>>;
template class InspectorToggleAttributeAction<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::physicsEnabled>>;

} // namespace nijigenerate::commands::inspector::apply_node
