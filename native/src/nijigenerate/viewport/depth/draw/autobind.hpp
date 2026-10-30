#pragma once
#include "session.hpp"
#include <compat/object_metadata.hpp>
#include <puppet.hpp>
#include <common/nullable_string.hpp>

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawAutoBindResult {
    DepthDrawString layerId, layerPath;
    DepthDrawString layerName, matchedNodeName, targetGridName;
    std::uint64_t matchedNodeUuid = 0, targetGridUuid = 0;
    bool matched = false, ambiguous = false;
    DepthDrawString status;
};
DepthDrawAutoBindResult ngDepthDrawAutoBindLayer(std::shared_ptr<nicxlive::core::Puppet> puppet,
    DepthDrawLayer layer, bool matchDirectGridName = true);
DepthDrawBinding ngDepthDrawBindingFromAutoBind(DepthDrawAutoBindResult result, int order = 0);
compat::Slice<DepthDrawAutoBindResult> ngDepthDrawAutoBindSession(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<nicxlive::core::Puppet> puppet, bool matchDirectGridName = true);
}

namespace nijigenerate::viewport::depth::draw::autobind {
using ::nijigenerate::viewport::depth::draw::DepthDrawAutoBindResult;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawAutoBindLayer;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawBindingFromAutoBind;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawAutoBindSession;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawAutoBindResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.autobind.DepthDrawAutoBindResult)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawAutoBindResult)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.autobind.DepthDrawAutoBindResult";
    inline static constexpr char unqualifiedElement[] = "DepthDrawAutoBindResult";
};
}
