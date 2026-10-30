#pragma once
#include "kra/document.hpp"
#include "inimport_fwd.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::io::kra {
template<class Document> struct Traits;
template<> struct Traits<KRA> {
    using Layer = ::nijigenerate::io::kra::Layer;
    using BlendingMode = ::nijigenerate::io::kra::BlendingMode;
    static compat::Slice<Layer> layers(KRA document);
    static bool isVisible(Layer layer);
    static bool isGroupStart(Layer layer);
    static bool isGroupEnd(Layer layer);
    static constexpr auto& parseDocument = kra::parseDocument;
};
bool incImportShowKRADialog();
// The source alias denotes the existing specialization, not a forwarding body.
inline constexpr auto& incAskImportKRA = inimport::incAskImport<KRA>;
} // namespace nijigenerate::io::kra

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::kra::Traits<::nijigenerate::io::kra::KRA>> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.kra.Traits!(KRA))";
    inline static constexpr char unqualifiedArgument[] = "(Traits!(KRA))";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.kra.Traits!(KRA)";
    inline static constexpr char unqualifiedElement[] = "Traits!(KRA)";
};
} // namespace nicxlive::core::d_runtime
