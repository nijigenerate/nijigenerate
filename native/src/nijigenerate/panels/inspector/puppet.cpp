#include "puppet.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <limits>
#include <bit>

namespace nijigenerate::panels::inspector::puppet {
using core::translation::ngTranslateCString;
namespace {
template<std::size_t N> compat::RetainedString translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
}
void PuppetInspector::inspect(std::shared_ptr<Parameter>, Cursor) {
    const auto receiver = retainSelf();
    if (mode == ModelEditSubMode::Layout && !targets.empty()) {
        const auto target = targets[0];
        const auto root = compat::ngRequireObject(project::incActivePuppet())->root;
        if (nicxlive::core::ngResourceObjectEquals(target, root)) run();
    }
}
void PuppetInspector::run() {
    const auto receiver = retainSelf();
    if (targets.empty()) return;
    const auto puppet = project::incActivePuppet();
    const auto root = compat::ngRequireObject(puppet)->root;
    using namespace widgets;
    ImGui::PushID(std::bit_cast<std::int32_t>(compat::ngRequireObject(root)->getUUID()));
    const auto icon = compat::RetainedString::fromStaticLiteral("\xee\xa4\xac");
    auto length = ngMeasureString(icon);
    ngText(translated("Puppet")); ImGui::SameLine(0,0);
    ngDummy(ImVec2(-length.x,length.y)); ImGui::SameLine(0,0); ngText(icon);
    ImGui::PopID(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();
    length = ngMeasureString(translated("nijilive Ver."));
    {
        const auto metadata = compat::ngRequireObject(puppet->meta);
        ngText(metadata->version_ ? std::string_view(*metadata->version_) : std::string_view{});
    }
    ImGui::SameLine(0,0); ngDummy(ImVec2(-length.x,length.y)); ImGui::SameLine(0,0);
    ngText(translated("nijilive Ver.")); ImGui::Spacing(); ImGui::Spacing();
    const auto field = [&]<std::size_t N>(const char* id, const char* title, const char (&tip)[N], const char* input,
                           auto member) {
        ImGui::PushID(id);
        // These source igTextColored calls interpret the translated label as a format string.
        ImGui::TextColored(CategoryTextColor, ngTranslateCString(title).data());
        ngTooltip(translated(tip));
        const auto metadata = compat::ngRequireObject(puppet->meta);
        ngInputText(input, metadata.get()->*member);
        ImGui::PopID();
    };
    if (ngBeginCategory(ngTranslateCString("General Info").data())) {
        ImGui::PushID("Part Count");
        ngTextColored(CategoryTextColor, translated("Part Count"));
        const auto count = core::translation::ngFormatArgument(
            compat::ngRequireObject(project::incActivePuppet())->getRootParts().size());
        ngTextColored(CategoryTextColor, core::translation::ngFormatTyped("%s", std::span(&count,1)));
        ImGui::PopID(); ImGui::Spacing();
        field("Name","Name","Name of the puppet","META_NAME",&nicxlive::core::PuppetMeta::name); ImGui::Spacing();
        field("Artists","Artist(s)","Artists who've drawn the puppet, seperated by comma","META_ARTISTS",&nicxlive::core::PuppetMeta::artist); ImGui::Spacing();
        field("Riggers","Rigger(s)","Riggers who've rigged the puppet, seperated by comma","META_RIGGERS",&nicxlive::core::PuppetMeta::rigger); ImGui::Spacing();
        field("Contact","Contact","Where to contact the main author of the puppet","META_CONTACT",&nicxlive::core::PuppetMeta::contact);
    }
    ngEndCategory();
    if (ngBeginCategory(ngTranslateCString("Licensing").data())) {
        field("LicenseURL","License URL","Link/URL to license","META_LICENSEURL",&nicxlive::core::PuppetMeta::licenseURL); ImGui::Spacing();
        field("Copyright","Copyright","Copyright holder information of the puppet","META_COPYRIGHT",&nicxlive::core::PuppetMeta::copyright); ImGui::Spacing();
        field("Origin","Origin","Where the model comes from on the internet.","META_ORIGIN",&nicxlive::core::PuppetMeta::reference);
    }
    ngEndCategory();
    if (ngBeginCategory(ngTranslateCString("Physics Globals").data())) {
        ImGui::PushID("PixelsPerMeter"); ngText(translated("Pixels per meter"));
        ngTooltip(translated("Number of pixels that correspond to 1 meter in the physics engine."));
        ngDragFloat(compat::RetainedString::fromStaticLiteral("PixelsPerMeter"),
            &compat::ngRequireObject(puppet->physics)->pixelsPerMeter,1,1,std::numeric_limits<float>::max(),
            compat::RetainedString::fromStaticLiteral("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        ImGui::PopID(); ImGui::Spacing();
        ImGui::PushID("Gravity"); ngText(translated("Gravity"));
        ngTooltip(translated("Acceleration due to gravity, in m/s². Earth gravity is 9.8."));
        {
            const auto physics = compat::ngRequireObject(puppet->physics);
            const auto format = translated("%.2f m/s²");
            ngDragFloat(compat::RetainedString::fromStaticLiteral("Gravity"), &physics->gravity, .01f, 0,
                std::numeric_limits<float>::max(), format, ImGuiSliderFlags_NoRoundToFormat);
        }
        ImGui::PopID();
    }
    ngEndCategory();
    if (ngBeginCategory(ngTranslateCString("Rendering Settings").data())) {
        ImGui::PushID("Filtering");
        const auto label = ngTranslateCString("Use Point Filtering");
        const auto active = compat::ngRequireObject(project::incActivePuppet());
        const auto metadata = compat::ngRequireObject(active->meta);
        if (ngCheckbox(label.data(), &metadata->preservePixels)) {
            compat::ngRequireObject(project::incActivePuppet())->populateTextureSlots();
            compat::ngRequireObject(project::incActivePuppet())->updateTextureState();
        }
        ngTooltip(translated("Makes nijilive model use point filtering, removing blur for low-resolution models."));
        ImGui::PopID();
    }
    ngEndCategory();
}
}
