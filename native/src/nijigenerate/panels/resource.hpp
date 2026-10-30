#pragma once
#include "package.hpp"
#include <nijigenerate/widgets/output_tree.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <serde_reader.hpp>
#include <serde_json.hpp>
#include <string_view>
#include <compat/object_metadata.hpp>

namespace nijigenerate::panels::resource {
class View : public std::enable_shared_from_this<View> {
public:
    compat::RetainedString command;
    std::shared_ptr<core::selector::TreeStore> store;
    std::shared_ptr<widgets::output::ListOutput> output;
    View(compat::RetainedString value,std::shared_ptr<widgets::output::ListOutput> list)
        :command(std::move(value)),output(std::move(list)) { prepareSourceClassInfo(); }
    virtual ~View() = default;
    View(const View&) = delete;
    View& operator=(const View&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<View> other);
    virtual int opCmp(std::shared_ptr<View> other);
protected:
    virtual void prepareSourceClassInfo();
};
class ResourcePanel : public Panel,public widgets::output::CommandIssuer,
        public std::enable_shared_from_this<ResourcePanel> {
    friend class ResourcePanelConfig;
    friend core::settings::SettingsJsonValue ngResourcePanelReadback();
    compat::Slice<std::shared_ptr<View>> history;
    bool forceUpdatePreview=false;
    std::uint32_t historyIndex=0;
    std::shared_ptr<nicxlive::core::Puppet> activePuppet;
    std::shared_ptr<nicxlive::core::nodes::Node> observedRoot;
    std::shared_ptr<nicxlive::core::param::Parameter> armedParameter;
    std::shared_ptr<widgets::output::ViewOutput> views;
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ResourcePanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.resource.ResourcePanel"));
            return true;
        }();
        (void)registered;
    }
    virtual void execFilter(std::shared_ptr<View>,bool refreshViews=true);
    virtual void notifyChange(std::shared_ptr<nicxlive::core::nodes::Node>,nicxlive::core::nodes::NotifyReason);
    void onUpdate() override;
public:
    ResourcePanel();
    ResourcePanel(const ResourcePanel&) = delete;
    ResourcePanel& operator=(const ResourcePanel&) = delete;
    void addCommand(compat::RetainedString) override;
    void setCommand(compat::RetainedString) override;
    void addPopup(core::selector::ResourcePtr) override;
};
class ResourcePanelConfig : public std::enable_shared_from_this<ResourcePanelConfig> {
public:
    // Compile-time metadata of the original @TypeId annotation; no runtime registry is required here.
    inline static constexpr std::string_view sourceTypeId="ResourcePanel";
    ResourcePanelConfig();
    virtual ~ResourcePanelConfig() = default;
    ResourcePanelConfig(const ResourcePanelConfig&) = delete;
    ResourcePanelConfig& operator=(const ResourcePanelConfig&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<ResourcePanelConfig> other);
    virtual int opCmp(std::shared_ptr<ResourcePanelConfig> other);
protected:
    virtual void prepareSourceClassInfo();
public:
    void serialize(nicxlive::core::serde::TypedJsonWriter&) const;
    virtual nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView);
};
void ngInitializeResourcePanelModule();
void ngInitResourcePanel();
void incLoadResourcePanel(std::shared_ptr<nicxlive::core::Puppet>);
void incDumpResourcePanelPath(std::shared_ptr<nicxlive::core::Puppet>);
core::settings::SettingsJsonValue ngResourcePanelReadback();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::panels::resource::ResourcePanel> {
    inline static constexpr char className[] = "nijigenerate.panels.resource.ResourcePanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.resource.ResourcePanel)";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::resource::View> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.resource.View)";
    inline static constexpr char unqualifiedArgument[] = "(View)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.resource.View";
    inline static constexpr char unqualifiedElement[] = "View";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::resource::View> {
    inline static constexpr char className[] = "nijigenerate.panels.resource.View";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.resource.View)";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::resource::ResourcePanelConfig> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.resource.ResourcePanelConfig)";
    inline static constexpr char unqualifiedArgument[] = "(ResourcePanelConfig)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.resource.ResourcePanelConfig";
    inline static constexpr char unqualifiedElement[] = "ResourcePanelConfig";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::resource::ResourcePanelConfig> {
    inline static constexpr char className[] = "nijigenerate.panels.resource.ResourcePanelConfig";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.resource.ResourcePanelConfig)";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::resource::ResourcePanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.resource.ResourcePanel)";
    inline static constexpr char unqualifiedArgument[] = "(ResourcePanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.resource.ResourcePanel";
    inline static constexpr char unqualifiedElement[] = "ResourcePanel";
};
}

#include <nijigenerate/core/typed_format.hpp>

namespace nijigenerate::core::translation {
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::panels::resource::View>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::panels::resource::View>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::panels::resource::ResourcePanelConfig>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::panels::resource::ResourcePanelConfig>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::panels::resource::ResourcePanel>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::panels::resource::ResourcePanel>> {};
}
