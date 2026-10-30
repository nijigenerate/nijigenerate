#pragma once
#include "session.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <compat/object_metadata.hpp>
#include <span>

namespace nijigenerate::viewport::depth::draw::manifest {
struct DepthDrawManifestValidation {
    compat::Slice<DepthDrawString> missingSourceLayerIds, missingTargetLayerIds, missingBindingLayerIds;
    compat::Slice<DepthDrawString> duplicateLayerIds, duplicateBindingKeys;
    DepthDrawString missingSelectedLayerId;
    std::uint64_t missingSelectedGridUuid = 0;
    bool ok() const;
};
core::settings::SettingsJsonValue ngDepthDrawSessionToManifest(std::shared_ptr<DepthDrawSession> session);
std::string ngDepthDrawManifestToJson(core::settings::SettingsJsonValue manifest);
void ngDepthDrawWriteFileBytes(DepthDrawString path, std::span<const std::uint8_t> bytes);
std::shared_ptr<DepthDrawSession> ngDepthDrawSessionFromManifest(core::settings::SettingsJsonValue manifest);
void ngSaveDepthDrawManifest(std::shared_ptr<DepthDrawSession> session, DepthDrawString path);
std::shared_ptr<DepthDrawSession> ngLoadDepthDrawManifest(DepthDrawString path);
DepthDrawManifestValidation ngValidateDepthDrawSessionManifest(std::shared_ptr<DepthDrawSession> session,
    compat::SharedDelegate<bool(std::uint64_t)> hasTargetGrid = {}, DepthDrawString sourceBaseDir = {});
} // namespace nijigenerate::viewport::depth::draw::manifest
namespace nijigenerate::viewport::depth::draw {
using manifest::DepthDrawManifestValidation;
using manifest::ngDepthDrawSessionToManifest;
using manifest::ngDepthDrawManifestToJson;
using manifest::ngDepthDrawWriteFileBytes;
using manifest::ngDepthDrawSessionFromManifest;
using manifest::ngSaveDepthDrawManifest;
using manifest::ngLoadDepthDrawManifest;
using manifest::ngValidateDepthDrawSessionManifest;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::draw::manifest::DepthDrawManifestValidation> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.manifest.DepthDrawManifestValidation)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawManifestValidation)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.manifest.DepthDrawManifestValidation";
    inline static constexpr char unqualifiedElement[] = "DepthDrawManifestValidation";
};
}
