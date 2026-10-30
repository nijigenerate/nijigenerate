#include "native.hpp"
#include "detail/native_maps.hpp"
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <nodes/projectable.hpp>
#include <cstdint>
#include <utility>

namespace nijigenerate::autorig::deterministic::native {

bool ngRigMaterialIsActive(std::shared_ptr<nicxlive::core::nodes::Part> part) {
    using namespace nicxlive::core::nodes;
    native_detail::NativeIntegerMap<std::uint32_t, bool> visiting;
    const auto active = [&](auto&& self, std::shared_ptr<Node> current) -> bool {
        if (!current || visiting.contains(current->getUUID())) return false;
        visiting[current->getUUID()] = true;
        bool result = false;
        compat::ngRunWithScopeExit([&] {
            result = [&]() -> bool {
                if (auto material = std::dynamic_pointer_cast<Part>(current))
                    if (material->opacity <= 0) return false;
                for (auto node = current; node; node = node->parentPtr()) {
                    if (!node->getEnabled()) return false;
                    if (auto composite = std::dynamic_pointer_cast<Projectable>(node))
                        if (composite->opacity <= 0) return false;
                    if (auto surface = std::dynamic_pointer_cast<Part>(node)) {
                        // Retain the Source foreach descriptor; only the first Mask is used.
                        const auto masks = surface->masks;
                        for (const auto mask : masks) {
                            if (mask.mode != MaskingMode::Mask) continue;
                            if (!self(self, compat::ngRequireObject(project::incActivePuppet())
                                    ->findNodeById(mask.maskSrcUUID))) return false;
                            break;
                        }
                    }
                }
                return true;
            }();
        }, [&] { visiting.erase(current->getUUID()); });
        return result;
    };
    return active(active, std::move(part));
}

} // namespace nijigenerate::autorig::deterministic::native
