#include "package.hpp"
#include <utility>

namespace nijigenerate::ext {
serde::ReadError ExPuppet::deserializeFromFghj(serde::TypedJsonView data) {
    const auto keepAlive = shared_from_this();
    const auto groupData = data.findFirst("groups");
    // fghj.byElement yields an empty range for non-arrays, including null.
    if (!groupData.isMissing() && groupData.kind() == serde::JsonKind::Array) {
        for (auto entry : groupData.elements())
            groups.append(std::dynamic_pointer_cast<ExParameterGroup>(
                nicxlive::core::param::inParameterCreate(entry)));
    }
    const auto manifest = data.findFirst("depthDrawSessionManifest");
    if (!manifest.isMissing()) {
        // The original deliberately ignores returned conversion and base errors.
        // A rejected conversion leaves the original immutable descriptor intact.
        (void)serde::deserializeValue(manifest, depthDrawSessionManifestJson);
    }
    (void)Puppet::deserializeFromFghj(data);
    return {};
}
void ExPuppet::serializeSelf(serde::TypedJsonWriter& serializer) {
    const auto keepAlive = shared_from_this();
    Puppet::serializeSelf(serializer);
    serializer.putKey("groups");
    serializer.serializeValue(groups);
    if (!depthDrawSessionManifestJson.empty()) {
        serializer.putKey("depthDrawSessionManifest");
        serializer.serializeValue(depthDrawSessionManifestJson.view());
    }
}
void ExPuppet::serializeSelf(serde::TypedJsonWriter& serializer) const {
    const auto keepAlive = shared_from_this();
    Puppet::serializeSelf(serializer);
    const auto groupValues = groups;
    serializer.putField("groups", groupValues);
    if (!depthDrawSessionManifestJson.empty()) {
        const auto manifest = depthDrawSessionManifestJson;
        serializer.putField("depthDrawSessionManifest", manifest.view());
    }
}
}
