#include "output.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <utility>

namespace nijigenerate::widgets::output {

ViewOutput::ViewOutput(CommandIssuerPtr value) {
    panel = std::move(value);
}

void ViewOutput::refresh(compat::Slice<core::selector::ResourcePtr> resources) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::Slice<core::selector::ResourcePtr> replacements;
    const auto currentNodes = nodes;
    for (auto node : currentNodes) {
        const auto candidates = resources;
        for (auto resource : candidates) {
            // Preserve both virtual getter calls and their source evaluation order.
            const auto replacementId = compat::ngRequireObject(resource)->uuid();
            const auto currentId = compat::ngRequireObject(node)->uuid();
            if (replacementId == currentId) replacements.append(resource);
        }
    }
    nodes = std::move(replacements);
}

void ViewOutput::addResources(compat::Slice<core::selector::ResourcePtr> resources) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    nodes.append(std::move(resources));
}

void ViewOutput::reset() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    nodes.resize(0);
    focused.reset();
}

} // namespace nijigenerate::widgets::output
