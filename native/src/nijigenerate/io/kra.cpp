// Independent source/nijigenerate/io/kra.d complete bodies.
#include "kra.hpp"
#include "package.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::io::kra {
compat::Slice<Layer> Traits<KRA>::layers(KRA document) { return document.layers; }
bool Traits<KRA>::isVisible(Layer layer) { return layer.isVisible; }
bool Traits<KRA>::isGroupStart(Layer layer) {
    // Keep the original (!type) == Any expression, including SectionDivider.
    return (!static_cast<std::int32_t>(layer.type)) == static_cast<std::int32_t>(LayerType::Any);
}
bool Traits<KRA>::isGroupEnd(Layer layer) { return layer.type == LayerType::SectionDivider; }
bool incImportShowKRADialog() {
    compat::Slice<TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.kra"};
    filters[0] = {{patterns.data(), patterns.size()}, "Krita Document (*.kra)"};
    auto file = io::incShowImportDialog({filters.data(), filters.size()}, core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Import...")));
    return inimport::incAskImport<KRA>(std::move(file));
}
} // namespace nijigenerate::io::kra
