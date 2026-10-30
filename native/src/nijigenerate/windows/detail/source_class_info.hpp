#pragma once
#include <compat/object_metadata.hpp>

// Exact class declarations from the independent nijigenerate.windows modules.
// Forward declarations keep base construction independent of derived targets.
namespace nijigenerate::windows {
namespace base { class Window; }
namespace about { class AboutWindow; }
namespace autosave { class RestoreSaveWindow; }
namespace command_browser { class CommandBrowserWindow; }
namespace editanim { class EditAnimationWindow; }
namespace flipconfig { class FlipPairWindow; }
namespace imgexport { class ImageExportWindow; }
namespace inpexport { class ExportWindow; }
namespace kramerge { class KRAMergeWindow; }
namespace parameditor { class ParamEditorWindow; }
namespace paramsplit { class ParamSplitWindow; }
namespace psddepthmap { class PSDDepthMapWindow; }
namespace psdmerge { class PSDMergeWindow; }
namespace rename { class RenameWindow; }
namespace settings { class SettingsWindow; }
namespace texviewer { class TextureViewerWindow; }
namespace videoexport { class VideoExportWindow; }
namespace welcome { class WelcomeWindow; }
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::base::Window> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.base.Window)";
    inline static constexpr char unqualifiedArgument[] = "(Window)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.base.Window";
    inline static constexpr char unqualifiedElement[] = "Window";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::base::Window> {
    inline static constexpr char className[] = "nijigenerate.windows.base.Window";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.base.Window)";
};
// These are plain, non-template Source class declarations, not RTTI spellings.
#define NG_WINDOW_SOURCE_CLASS(module, name) \
    template<> struct SourceObjectTypeNames<::nijigenerate::windows::module::name> { \
        inline static constexpr char className[] = "nijigenerate.windows." #module "." #name; \
        inline static constexpr char templateArgumentName[] = "nijigenerate.windows." #module "." #name; \
    }
template<> struct SourceTypeSpellings<::nijigenerate::windows::about::AboutWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.about.AboutWindow)";
    inline static constexpr char unqualifiedArgument[] = "(AboutWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.about.AboutWindow";
    inline static constexpr char unqualifiedElement[] = "AboutWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::about::AboutWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.about.AboutWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.about.AboutWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::autosave::RestoreSaveWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.autosave.RestoreSaveWindow)";
    inline static constexpr char unqualifiedArgument[] = "(RestoreSaveWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.autosave.RestoreSaveWindow";
    inline static constexpr char unqualifiedElement[] = "RestoreSaveWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::autosave::RestoreSaveWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.autosave.RestoreSaveWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.autosave.RestoreSaveWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::command_browser::CommandBrowserWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.command_browser.CommandBrowserWindow)";
    inline static constexpr char unqualifiedArgument[] = "(CommandBrowserWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.command_browser.CommandBrowserWindow";
    inline static constexpr char unqualifiedElement[] = "CommandBrowserWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::command_browser::CommandBrowserWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.command_browser.CommandBrowserWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.command_browser.CommandBrowserWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::editanim::EditAnimationWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.editanim.EditAnimationWindow)";
    inline static constexpr char unqualifiedArgument[] = "(EditAnimationWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.editanim.EditAnimationWindow";
    inline static constexpr char unqualifiedElement[] = "EditAnimationWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::editanim::EditAnimationWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.editanim.EditAnimationWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.editanim.EditAnimationWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::flipconfig::FlipPairWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.flipconfig.FlipPairWindow)";
    inline static constexpr char unqualifiedArgument[] = "(FlipPairWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.flipconfig.FlipPairWindow";
    inline static constexpr char unqualifiedElement[] = "FlipPairWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::flipconfig::FlipPairWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.flipconfig.FlipPairWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.flipconfig.FlipPairWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::imgexport::ImageExportWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.imgexport.ImageExportWindow)";
    inline static constexpr char unqualifiedArgument[] = "(ImageExportWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.imgexport.ImageExportWindow";
    inline static constexpr char unqualifiedElement[] = "ImageExportWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::imgexport::ImageExportWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.imgexport.ImageExportWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.imgexport.ImageExportWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::inpexport::ExportWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.inpexport.ExportWindow)";
    inline static constexpr char unqualifiedArgument[] = "(ExportWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.inpexport.ExportWindow";
    inline static constexpr char unqualifiedElement[] = "ExportWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::inpexport::ExportWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.inpexport.ExportWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.inpexport.ExportWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::kramerge::KRAMergeWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.kramerge.KRAMergeWindow)";
    inline static constexpr char unqualifiedArgument[] = "(KRAMergeWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.kramerge.KRAMergeWindow";
    inline static constexpr char unqualifiedElement[] = "KRAMergeWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::kramerge::KRAMergeWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.kramerge.KRAMergeWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.kramerge.KRAMergeWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::parameditor::ParamEditorWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.parameditor.ParamEditorWindow)";
    inline static constexpr char unqualifiedArgument[] = "(ParamEditorWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.parameditor.ParamEditorWindow";
    inline static constexpr char unqualifiedElement[] = "ParamEditorWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::parameditor::ParamEditorWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.parameditor.ParamEditorWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.parameditor.ParamEditorWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::paramsplit::ParamSplitWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.paramsplit.ParamSplitWindow)";
    inline static constexpr char unqualifiedArgument[] = "(ParamSplitWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.paramsplit.ParamSplitWindow";
    inline static constexpr char unqualifiedElement[] = "ParamSplitWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::paramsplit::ParamSplitWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.paramsplit.ParamSplitWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.paramsplit.ParamSplitWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PSDDepthMapWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PSDDepthMapWindow)";
    inline static constexpr char unqualifiedArgument[] = "(PSDDepthMapWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PSDDepthMapWindow";
    inline static constexpr char unqualifiedElement[] = "PSDDepthMapWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::psddepthmap::PSDDepthMapWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.psddepthmap.PSDDepthMapWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.psddepthmap.PSDDepthMapWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psdmerge::PSDMergeWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psdmerge.PSDMergeWindow)";
    inline static constexpr char unqualifiedArgument[] = "(PSDMergeWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psdmerge.PSDMergeWindow";
    inline static constexpr char unqualifiedElement[] = "PSDMergeWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::psdmerge::PSDMergeWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.psdmerge.PSDMergeWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.psdmerge.PSDMergeWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::rename::RenameWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.rename.RenameWindow)";
    inline static constexpr char unqualifiedArgument[] = "(RenameWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.rename.RenameWindow";
    inline static constexpr char unqualifiedElement[] = "RenameWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::rename::RenameWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.rename.RenameWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.rename.RenameWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::settings::SettingsWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.settings.SettingsWindow)";
    inline static constexpr char unqualifiedArgument[] = "(SettingsWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.settings.SettingsWindow";
    inline static constexpr char unqualifiedElement[] = "SettingsWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::settings::SettingsWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.settings.SettingsWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.settings.SettingsWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::texviewer::TextureViewerWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.texviewer.TextureViewerWindow)";
    inline static constexpr char unqualifiedArgument[] = "(TextureViewerWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.texviewer.TextureViewerWindow";
    inline static constexpr char unqualifiedElement[] = "TextureViewerWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::texviewer::TextureViewerWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.texviewer.TextureViewerWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.texviewer.TextureViewerWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::videoexport::VideoExportWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.videoexport.VideoExportWindow)";
    inline static constexpr char unqualifiedArgument[] = "(VideoExportWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.videoexport.VideoExportWindow";
    inline static constexpr char unqualifiedElement[] = "VideoExportWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::videoexport::VideoExportWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.videoexport.VideoExportWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.videoexport.VideoExportWindow)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::welcome::WelcomeWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.welcome.WelcomeWindow)";
    inline static constexpr char unqualifiedArgument[] = "(WelcomeWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.welcome.WelcomeWindow";
    inline static constexpr char unqualifiedElement[] = "WelcomeWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::welcome::WelcomeWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.welcome.WelcomeWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.welcome.WelcomeWindow)";
};
#undef NG_WINDOW_SOURCE_CLASS
}

namespace nijigenerate::windows::base::detail {
template<class T> void ngRegisterSourceWindowClass() {
    using namespace nicxlive::core::d_runtime;
    if constexpr (requires { SourceObjectTypeNames<T>::className(); } ||
            requires { SourceObjectTypeNames<T>::className; }) {
        static const bool registered = [] {
            if constexpr (requires { SourceObjectTypeNames<T>::className(); }) {
                ngRegisterObjectClassName(typeid(T), RetainedString(SourceObjectTypeNames<T>::className()));
            } else {
                ngRegisterObjectClassName(typeid(T),
                    RetainedString::fromStaticLiteral(SourceObjectTypeNames<T>::className));
            }
            return true;
        }();
        (void)registered;
    }
    // A native-only subclass may register its own genuine Source metadata.
    // Missing metadata is never replaced by a base name or an RTTI spelling.
}
}
