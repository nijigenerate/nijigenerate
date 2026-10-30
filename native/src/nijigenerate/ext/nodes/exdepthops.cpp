#include "nijigenerate/ext/nodes/exdepthops.hpp"

#include <nodes/node.hpp>

namespace nijigenerate::ext::nodes {

void ExDepthOperated::copyDepthOpsFrom(std::shared_ptr<nicxlive::core::nodes::Node> source) {
    if (auto* operated = dynamic_cast<DepthOperationMappedNode*>(source.get())) {
        depthOps = operated->copyDepthOps();
        depthOpBaseDepths = operated->copyDepthOpBaseDepths();
    } else {
        depthOps.clear();
        depthOpBaseDepths.clear();
    }
}

} // namespace nijigenerate::ext::nodes
