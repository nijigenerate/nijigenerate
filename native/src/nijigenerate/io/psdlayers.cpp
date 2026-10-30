#include "nijigenerate/io/psdlayers.hpp"
#include "nijigenerate/io/psd/layer.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <string>
#include <stdexcept>

namespace nijigenerate::io::psdlayers {
namespace {
char& joinedAt(std::span<char> values, std::size_t index, std::size_t sourceLine) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("std/array.d"), sourceLine);
    return values[index];
}
}
compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(compat::Slice<psd::Layer> layers) {
    return ngPsdLayerGroupStates(std::span<const psd::Layer>(layers.data(), layers.size()));
}

compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(compat::ConstSlice<psd::Layer> layers) {
    return ngPsdLayerGroupStates(layers.view());
}

compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(std::span<const psd::Layer> layers) {
    compat::Slice<PsdLayerGroupState> states;
    states.resize(layers.size());
    compat::Slice<compat::RetainedString> pathSegments;
    compat::Slice<bool> visibility;
    compat::Slice<float> opacity;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        switch (layer.type) {
        case psd::LayerType::OpenFolder:
        case psd::LayerType::ClosedFolder:
            pathSegments.append(layer.name);
            visibility.append((visibility.empty() || visibility[visibility.size() - 1]) &&
                (layer.flags & psd::LayerFlags::Visible) == 0);
            opacity.append((opacity.empty() ? 1.0f : opacity[opacity.size() - 1]) *
                static_cast<float>(layer.opacity) / 255.0f);
            continue;
        case psd::LayerType::SectionDivider:
            if (!pathSegments.empty()) {
                pathSegments.resize(pathSegments.size() - 1);
                visibility.resize(visibility.size() - 1);
                opacity.resize(opacity.size() - 1);
            }
            continue;
        case psd::LayerType::Any:
            if (pathSegments.empty()) states[i].path = compat::RetainedString::emptyLiteral();
            else {
                // std.array.join captures the input descriptor and sizes its result first.
                const auto segments = pathSegments;
                std::size_t joinedLength = 0;
                for (const auto segment : segments) joinedLength += segment.size();
                joinedLength += segments.size() - 1;
                // The join result is uninitialized storage, filled before publication.
                std::shared_ptr<char[]> joinedStorage;
                if (joinedLength) {
                    try {
                        joinedStorage = std::make_shared_for_overwrite<char[]>(joinedLength);
                    } catch (const std::bad_alloc&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    }
                }
                const std::span<char> joinedBytes(joinedStorage.get(), joinedLength);
                std::size_t cursor = 0;
                for (std::size_t segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
                    if (segmentIndex != 0) joinedAt(joinedBytes, cursor++, 2263) = '/';
                    const auto segment = segments[segmentIndex];
                    for (const char byte : segment.view())
                        joinedAt(joinedBytes, cursor++, segmentIndex == 0 ? 2258 : 2265) = byte;
                }
#ifndef NDEBUG
                if (cursor != joinedLength) throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("Assertion failure"),
                    compat::RetainedString::fromStaticLiteral("std/array.d"), 2267);
#endif
                const auto joined = joinedLength == 0 ? compat::RetainedString{}
                    : compat::RetainedString::retainedView(
                        std::shared_ptr<const void>(joinedStorage, joinedStorage.get()),
                        joinedBytes.data(), joinedLength);
                // The leading slash is a separate concatenation after join completes.
                states[i].path = [&] {
                    try {
                        std::string path("/");
                        if (!joined.empty()) path.append(joined.view());
                        return compat::RetainedString(std::move(path));
                    } catch (const std::bad_alloc&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    } catch (const std::length_error&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    }
                }();
            }
            states[i].visible = visibility.empty() || visibility[visibility.size() - 1];
            states[i].opacity = opacity.empty() ? 1.0f : opacity[opacity.size() - 1];
            continue;
        }
        // Use the canonical source staticError at the actual final-switch location.
        throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/psdlayers.d"), 20);
    }
    return states;
}
} // namespace nijigenerate::io::psdlayers
