#include "psd.hpp"
#include "package.hpp"
#include <nijigenerate/core/translation.hpp>
#include <algorithm>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::io::psd {
compat::Slice<Layer> Traits<PSD>::layers(PSD document) {
    auto result = document.layers.duplicate();
    const auto validHierarchy = [](const compat::Slice<Layer> records) {
        std::size_t depth = 0;
        for (const auto layer : records) {
            if (layer.type == LayerType::OpenFolder || layer.type == LayerType::ClosedFolder) ++depth;
            else if (layer.type == LayerType::SectionDivider) {
                if (!depth) return false;
                --depth;
            }
        }
        return depth == 0;
    };
    if (!validHierarchy(result)) {
        if (!result.empty()) std::reverse(result.begin(), result.end());
        if (!validHierarchy(result)) {
            compat::RetainedString message;
            try {
                message = compat::RetainedString(std::string_view(
                    "PSD folder records do not form a balanced hierarchy"));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
            throw compat::ThrowableException(std::move(message),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/psd.d"),47);
        }
    } else {
        bool hasFolders = false;
        for (const auto layer : result)
            if (layer.type == LayerType::OpenFolder || layer.type == LayerType::ClosedFolder ||
                layer.type == LayerType::SectionDivider) hasFolders = true;
        if (!hasFolders) {
            const auto validClipping = [](const compat::Slice<Layer> records) {
                bool hasBase = false;
                for (std::size_t offset = records.size(); offset != 0; --offset) {
                    const auto layer = records[offset - 1];
                    if (isClippingLayer(layer)) {
                        if (!hasBase) return false;
                    } else hasBase = true;
                }
                return true;
            };
            if (!validClipping(result)) {
                auto reversed = result.duplicate();
                if (!reversed.empty()) std::reverse(reversed.begin(), reversed.end());
                if (validClipping(reversed)) result = reversed;
            }
        }
    }
    return result;
}
bool Traits<PSD>::isVisible(Layer layer) { return (layer.flags & LayerFlags::Visible) == 0; }
bool Traits<PSD>::isGroupStart(Layer layer) {
    return layer.type == LayerType::OpenFolder || layer.type == LayerType::ClosedFolder;
}
bool Traits<PSD>::isGroupEnd(Layer layer) { return layer.type == LayerType::SectionDivider; }
bool Traits<PSD>::isClippingLayer(Layer layer) { return !layer.clipping; }
bool Traits<PSD>::isPassThroughGroup(Layer layer) { return layer.blendModeKey == BlendingMode::PassThrough; }
bool incImportShowPSDDialog() {
    compat::Slice<TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.psd"};
    filters[0] = {{patterns.data(), patterns.size()}, "Photoshop Document (*.psd)"};
    auto file = io::incShowImportDialog({filters.data(), filters.size()},
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Import...")));
    return inimport::incAskImport<PSD>(std::move(file));
}
}
