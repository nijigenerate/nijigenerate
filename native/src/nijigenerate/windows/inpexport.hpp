#pragma once
#include "base.hpp"
#include <nijigenerate/io/inpexport.hpp>

namespace nijigenerate::windows::inpexport {
// D's enum has a string underlying value, including values cast from other strings.
class ExportOptionsPane {
    friend class ExportWindow;
    compat::RetainedString value_;
public:
    ExportOptionsPane() : value_(compat::RetainedString::fromStaticLiteral("Atlassing")) {}
    explicit ExportOptionsPane(compat::RetainedString value) : value_(std::move(value)) {}
    std::string_view value() const noexcept { return value_.view(); }
    friend bool operator==(const ExportOptionsPane&, const ExportOptionsPane&) = default;
    static const ExportOptionsPane Atlassing;
    static const ExportOptionsPane Decoration;
};
inline const ExportOptionsPane ExportOptionsPane::Atlassing{compat::RetainedString::fromStaticLiteral("Atlassing")};
inline const ExportOptionsPane ExportOptionsPane::Decoration{compat::RetainedString::fromStaticLiteral("Decoration")};
struct ExportOptions {
    std::size_t atlasResolution = 2048;
    bool nonLinearScaling = false;
    float resolutionScale = 1;
    int padding = 16;
};
class ExportWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    compat::RetainedString outFile;
    ExportOptionsPane pane = ExportOptionsPane::Atlassing;
    io::inpexport::IncINPPreviewInfo preview;
    bool wasScaledForced = false;
    io::inpexport::IncINPExportSettings settings;
    const char* resolutionScaleString = "100%";
    const char* atlasResolutionString = "2048x2048";
    compat::RetainedString atlasResolutionStringOwner;
    void beginSection(compat::RetainedStringSlice);
    void endSection();
    void regenPreview();
    void setBlending(nicxlive::core::nodes::BlendMode);
    void loadWatermark(compat::RetainedString);
    void initialize(compat::RetainedString);
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    ExportWindow() = default;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::inpexport::ExportOptionsPane> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.inpexport.ExportOptionsPane)";
    inline static constexpr char unqualifiedArgument[] = "(ExportOptionsPane)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.inpexport.ExportOptionsPane";
    inline static constexpr char unqualifiedElement[] = "ExportOptionsPane";
};

template<> struct SourceTypeSpellings<::nijigenerate::windows::inpexport::ExportOptions> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.inpexport.ExportOptions)";
    inline static constexpr char unqualifiedArgument[] = "(ExportOptions)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.inpexport.ExportOptions";
    inline static constexpr char unqualifiedElement[] = "ExportOptions";
};
}
