#include "repair.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <puppet.hpp>
#include <texture.hpp>

namespace nijigenerate::utils::repair {
void ngAttemptRepairPuppet(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    ngRegenerateNodeIDs(compat::ngRequireObject(puppet)->root);
    puppet->rescanNodes();
}
void ngPremultTextures(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    // The original foreach captures an array descriptor and references each slot.
    auto slots = compat::ngRequireObject(puppet)->textureSlots;
    for (auto& texture : slots) {
        auto data = compat::ngRequireObject(texture)->getSourceTextureData();
        core::texture_pixels::ngTexPremultiply(data);
        compat::ngRequireObject(texture)->setData(data);
    }
}
}
