#pragma once
#include "psd/document.hpp"
#include "inimport_fwd.hpp"
#include <compat/object_metadata.hpp>
#include <utility>

namespace nijigenerate::io::psd {
template<class Document> struct Traits;
template<> struct Traits<PSD> {
    using Layer = ::nijigenerate::io::psd::Layer;
    using LayerType = ::nijigenerate::io::psd::LayerType;
    using BlendingMode = ::nijigenerate::io::psd::BlendingMode;
    static constexpr bool layersTopToBottom = true;
    static compat::Slice<Layer> layers(PSD document);
    static bool isVisible(Layer layer);
    static bool isGroupStart(Layer layer);
    static bool isGroupEnd(Layer layer);
    static bool isClippingLayer(Layer layer);
    static bool isPassThroughGroup(Layer layer);
    // C++ cannot import a namespace overload set into a class with using.
    // Calls and explicitly typed addresses still denote the two genuine parsers.
    struct ParseDocumentAlias {
        using NameParser = PSD (*)(compat::RetainedString);
        using FileParser = PSD (*)(RetainedFile&);
        constexpr operator NameParser() const noexcept {
            return static_cast<NameParser>(&::nijigenerate::io::psd::parseDocument);
        }
        constexpr operator FileParser() const noexcept {
            return static_cast<FileParser>(&::nijigenerate::io::psd::parseDocument);
        }
        PSD operator()(compat::RetainedString file) const {
            return static_cast<NameParser>(*this)(std::move(file));
        }
        PSD operator()(RetainedFile& file) const {
            return static_cast<FileParser>(*this)(file);
        }
        constexpr ParseDocumentAlias operator&() const noexcept { return {}; }
    };
    inline static constexpr ParseDocumentAlias parseDocument{};
};
bool incImportShowPSDDialog();
inline constexpr auto& incAskImportPSD = inimport::incAskImport<PSD>;
} // namespace nijigenerate::io::psd

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::psd::Traits<::nijigenerate::io::psd::PSD>> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.psd.Traits!(PSD))";
    inline static constexpr char unqualifiedArgument[] = "(Traits!(PSD))";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.psd.Traits!(PSD)";
    inline static constexpr char unqualifiedElement[] = "Traits!(PSD)";
};
} // namespace nicxlive::core::d_runtime
