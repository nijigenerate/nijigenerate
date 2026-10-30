#include "file_helpers.hpp"
#include "file_allocation.hpp"
#include "nijigenerate/commands/command.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include <puppet.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <utility>

namespace nijigenerate::commands::puppet::file::detail {

std::shared_ptr<CommandResult> ngMissingPathResult(compat::RetainedStringSlice what) {
    auto message = fileAllocation([&] {
        std::string text(what.view());
        text += " not provided";
        return compat::RetainedString(std::move(text));
    });
    return CommandResult::opCall(false, std::move(message));
}

compat::RetainedStringSlice ngUniqueAnimationName(std::shared_ptr<nicxlive::core::Puppet> puppet,
                                                 compat::RetainedStringSlice baseName) {
    auto candidate = baseName.empty() ?
        compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("Imported")) : baseName;
    {
        const auto& animations = puppet->getAnimations();
        if (animations.find(candidate.retainedString()) == animations.end()) return candidate;
    }
    std::int32_t index = 2;
    for (;;) {
        const std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(candidate), core::translation::ngFormatArgument(index)};
        auto next = core::translation::ngFormatTyped("%s (%s)", arguments);
        const auto& animations = puppet->getAnimations();
        if (animations.find(next) == animations.end())
            return fileAllocation([&] { return compat::RetainedString(std::move(next)); });
        index = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(index) + std::uint32_t{1});
    }
}

} // namespace nijigenerate::commands::puppet::file::detail
