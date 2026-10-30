#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>

namespace nijigenerate::panels {
using PanelString = compat::RetainedString;

class Panel {
public:
    Panel(PanelString name, PanelString displayName, bool defaultVisibility);
    template<class T> requires std::derived_from<T, Panel>
    static std::shared_ptr<T> createSource() {
        auto result = std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
        static_cast<Panel&>(*result).sourceOwner_ = result;
        return result;
    }
    virtual ~Panel() = default;
    virtual PanelString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Panel> other);
    virtual int opCmp(std::shared_ptr<Panel> other);
    bool visible;
    bool alwaysVisible = false;
    virtual void init_() final;
    virtual PanelString name() const final;
    virtual PanelString displayName() const final;
    // Retains the source toStringz allocation when the pointer escapes/reinit occurs.
    virtual PanelString displayNameC() const final;
    virtual bool isActive();
    virtual void update() final;
    virtual bool getDefaultVisibility() const final;
protected:
    virtual void prepareSourceClassInfo();
    ImVec2 panelSpace{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    project::EditMode activeModes = project::EditMode::ALL;
    bool supportsReadOnlyInteraction = false;
    virtual void onUpdate() = 0;
    virtual void onBeginUpdate();
    virtual void onEndUpdate();
    virtual void onInit();
private:
    friend void incAddPanel(std::shared_ptr<Panel> panel);
    // Bind the actual control block without adding another enable_shared_from_this
    // base: existing concrete panels already own their typed shared-owner base.
    std::weak_ptr<Panel> sourceOwner_;
    PanelString name_;
    PanelString displayName_;
    bool defaultVisibility;
    PanelString windowID;
    PanelString displayNamePtr;
    bool drewContents = false;
    bool wasVisible = false;
};

using PanelPtr = std::shared_ptr<Panel>;
using PanelSlice = compat::Slice<PanelPtr>;
extern thread_local PanelSlice incPanels;
void incAddPanel(PanelPtr panel);
void incUpdatePanels();
void incInitPanels();
PanelPtr incFindPanelByName(PanelString name);

// Explicit native module entry replaces the D per-thread module static ctor.
// The owning import catalogue calls this before application/thread entry; no
// panel reference is retained after a registry clear just to remember registration.
template<class T> requires std::derived_from<T, Panel> && std::default_initializable<T>
void ngInitializePanelModule() {
    thread_local bool initialized = false;
    if (!initialized) {
        using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<T>;
        if constexpr (requires { Names::className(); }) {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                PanelString(Names::className()));
        } else if constexpr (requires { Names::className; }) {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                PanelString::fromStaticLiteral(Names::className));
        } else if (const auto name = nicxlive::core::d_runtime::ngFindObjectClassName(
                typeid(nicxlive::core::d_runtime::SourceObjectTypeIdentity<T>))) {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T), *name);
        }
        incAddPanel(Panel::createSource<T>());
        initialized = true;
    }
}
} // namespace nijigenerate::panels

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::Panel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.Panel)";
    inline static constexpr char unqualifiedArgument[] = "(Panel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.Panel";
    inline static constexpr char unqualifiedElement[] = "Panel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::Panel> {
    inline static constexpr char className[] = "nijigenerate.panels.Panel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.Panel)";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {
template<> struct FormatObjectTraits<::nijigenerate::panels::PanelPtr>
    : detail::SourceNoArgObjectFormat<::nijigenerate::panels::PanelPtr> {};
} // namespace nijigenerate::core::translation
