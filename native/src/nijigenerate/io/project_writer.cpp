#include "project_writer.hpp"
#include "imagefmt/writer.hpp"
#include <fmt/fmt.hpp>
#include <serde_json.hpp>
#include <span>
#include <type_traits>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::io {
namespace {
template<class PuppetType> std::vector<std::uint8_t> writePuppetMemory(PuppetType& puppet) {
    using namespace nicxlive::core;
    [[maybe_unused]] const auto receiverLifetime = puppet.weak_from_this().lock();
    fmt::inpModeFlag() = true;
    // Current independently authored nijilive/ver.d input; no VCS query is performed.
    compat::ngRequireObject(puppet.meta)->version_ = "v1.0.0-alpha1-102-g478654d";
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(puppet);
    serializer.flush();
    const auto& json = serializer.str();
    std::vector<std::uint8_t> output;
    const auto append32 = [&](std::uint32_t value) {
        output.push_back(static_cast<std::uint8_t>(value >> 24));
        output.push_back(static_cast<std::uint8_t>(value >> 16));
        output.push_back(static_cast<std::uint8_t>(value >> 8));
        output.push_back(static_cast<std::uint8_t>(value));
    };
    output.insert(output.end(), fmt::MAGIC_BYTES.begin(), fmt::MAGIC_BYTES.end());
    append32(static_cast<std::uint32_t>(json.size()));
    output.insert(output.end(), json.begin(), json.end());
    output.insert(output.end(), fmt::TEX_SECTION.begin(), fmt::TEX_SECTION.end());
    append32(static_cast<std::uint32_t>(puppet.textureSlots.size()));
    const auto slots = puppet.textureSlots;
    for (const auto texture : slots) {
        const auto width = compat::ngRequireObject(texture)->width();
        const auto height = compat::ngRequireObject(texture)->height();
        const auto pixels = [&] {
            if constexpr (std::is_const_v<PuppetType>)
                return SourceTextureBytes(compat::ngRequireObject(texture)->getTextureData());
            else
                return compat::ngRequireObject(texture)->getSourceTextureData();
        }();
        const auto input = std::span<const std::uint8_t>(pixels.data(), pixels.size());
        const auto channels = compat::ngRequireObject(texture)->channels();
        auto image = imagefmt::writeImageMemory(imagefmt::tga, width, height, input, channels);
        // The original writer intentionally ignores imagefmt's error out-parameter.
        append32(static_cast<std::uint32_t>(image.length));
        output.push_back(1); // IN_TEX_TGA
        if (image.length) output.insert(output.end(), image.buffer.data(), image.buffer.data() + image.length);
    }
    if (!puppet.extData.empty()) {
        output.insert(output.end(), fmt::EXT_SECTION.begin(), fmt::EXT_SECTION.end());
        append32(static_cast<std::uint32_t>(puppet.extData.size()));
        const auto extensions = puppet.extData;
        for (const auto& entry : extensions) {
            const auto name = entry.first;
            const auto payload = entry.second;
            append32(static_cast<std::uint32_t>(name.size()));
            const auto nameBytes = name.view();
            if (!nameBytes.empty()) output.insert(output.end(), nameBytes.begin(), nameBytes.end());
            append32(static_cast<std::uint32_t>(payload.size()));
            if (!payload.empty()) output.insert(output.end(), payload.begin(), payload.end());
        }
    }
    return output;
}
}
std::vector<std::uint8_t> ngWriteProjectPuppetMemory(const nicxlive::core::Puppet& puppet) {
    return writePuppetMemory(puppet);
}
std::vector<std::uint8_t> ngWriteProjectPuppetMemory(nicxlive::core::Puppet& puppet) {
    return writePuppetMemory(puppet);
}
}
