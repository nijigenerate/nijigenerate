#include "templates.hpp"
#include "detail/embedded_templates.hpp"
#include "detail/sha256.hpp"
#include "contracts_file.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <common/string_key_map.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cstddef>
#include <string>
#include <array>

namespace nijigenerate::autorig::deterministic::templates {
namespace {
using Value = core::settings::SettingsJsonValue;
using Resources = nicxlive::core::common::StringKeyMap<compat::RetainedString>;

Resources initializeResources() {
    Resources result;
    result.setRetained(compat::RetainedString::fromStaticLiteral("face_head"), detail::embedded0());
    result.setRetained(compat::RetainedString::fromStaticLiteral("eye_opening"), detail::embedded1());
    result.setRetained(compat::RetainedString::fromStaticLiteral("mouth_opening"), detail::embedded2());
    result.setRetained(compat::RetainedString::fromStaticLiteral("torso"), detail::embedded3());
    result.setRetained(compat::RetainedString::fromStaticLiteral("neck"), detail::embedded4());
    result.setRetained(compat::RetainedString::fromStaticLiteral("shoulder"), detail::embedded5());
    result.setRetained(compat::RetainedString::fromStaticLiteral("arm"), detail::embedded6());
    result.setRetained(compat::RetainedString::fromStaticLiteral("leg"), detail::embedded7());
    result.setRetained(compat::RetainedString::fromStaticLiteral("hand"), detail::embedded8());
    result.setRetained(compat::RetainedString::fromStaticLiteral("foot"), detail::embedded9());
    result.setRetained(compat::RetainedString::fromStaticLiteral("skirt"), detail::embedded10());
    result.setRetained(compat::RetainedString::fromStaticLiteral("sleeve"), detail::embedded11());
    result.setRetained(compat::RetainedString::fromStaticLiteral("hanging_sheet"), detail::embedded12());
    result.setRetained(compat::RetainedString::fromStaticLiteral("tube"), detail::embedded13());
    result.setRetained(compat::RetainedString::fromStaticLiteral("leaf"), detail::embedded14());
    result.setRetained(compat::RetainedString::fromStaticLiteral("ribbon_network"), detail::embedded15());
    result.setRetained(compat::RetainedString::fromStaticLiteral("frill"), detail::embedded16());
    result.setRetained(compat::RetainedString::fromStaticLiteral("surface_layer"), detail::embedded17());
    result.setRetained(compat::RetainedString::fromStaticLiteral("rigid_panel"), detail::embedded18());
    return result;
}

// Process-shared initialization; cross-module startup ordering remains a module gate.
const Resources resources = initializeResources();

const auto sourceFile = compat::RetainedString::fromStaticLiteral(
    "source/nijigenerate/autorig/deterministic/templates.d");

template<class Message> void enforce(bool condition, Message&& message, std::size_t line) {
    if (!condition) {
        const auto evaluated = message();
        throw compat::ThrowableException(contracts::detail::copyContractMessage(evaluated.view()), sourceFile, line);
    }
}

Value member(const Value& value, std::string_view key) {
    return value.sourceIndexValue(key);
}

compat::RetainedString concatenate(std::string_view prefix, std::string_view suffix) {
    return contracts::detail::allocateContractStorage([&] {
        std::string bytes(prefix); bytes.append(suffix);
        return compat::RetainedString(std::move(bytes));
    });
}

Value parse(compat::RetainedString text) {
    return autorig::json::detail::FirstPassReader(compat::RetainedStringSlice(text)).parseRoot();
}
} // namespace

Value ngRigTemplate(compat::RetainedStringSlice id) {
    const auto* source = resources.find(id.view());
    enforce(source != nullptr, [&] {
        return concatenate("Unknown embedded rig template: ",id.view());
    }, 41);
    auto result = parse(*source);
    enforce(member(result, "id").as_string().view() == id.view()
        && member(result, "schema_version").as_string().view() == "semantic-grid-template/1",
        [] { return compat::RetainedString::fromStaticLiteral("Invalid embedded rig template identity"); }, 43);
    return result;
}

Value ngRigTemplateManifest() { return parse(detail::embedded19()); }
Value ngRigTemplateSchema() { return parse(detail::embedded20()); }
Value ngRigHumanoidPrior() { return parse(detail::embedded21()); }
Value ngRigMaterialRoles() { return parse(detail::embedded22()); }

void ngRigValidateEmbeddedTemplates() {
    auto manifest = ngRigTemplateManifest();
    enforce(member(manifest, "templates").as_array().size() == resources.size(),
        [] { return compat::RetainedString::fromStaticLiteral("Embedded template count mismatch"); }, 66);
    nicxlive::core::common::StringKeyMap<bool> seen;
    const auto entries = member(manifest, "templates").as_array();
    for (auto entry : entries) {
        const auto id = member(entry, "id").as_string();
        enforce(!seen.contains(id.view()), [] {
            return compat::RetainedString::fromStaticLiteral("Duplicate embedded template identity");
        }, 70);
        seen.setRetained(id.retainedString(), true);
        const auto* source = resources.find(id.view());
        enforce(source != nullptr, [&] {
            return concatenate("Manifest template is missing: ",id.view());
        }, 73);
        contracts::detail::Sha256 hash;
        hash.put(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(source->view().data()),
            source->view().size()));
        const auto bytes = hash.finish();
        std::array<char,64> hexadecimal{};
        std::size_t offset = 0;
        constexpr char hex[] = "0123456789ABCDEF";
        for (auto byte : bytes) {
            hexadecimal[offset++] = hex[byte >> 4];
            hexadecimal[offset++] = hex[byte & 15];
        }
        auto digest = contracts::detail::allocateContractStorage([&] {
            return std::string(hexadecimal.data(),hexadecimal.size());
        });
        for (auto& digit : digest)
            if (digit >= 'A' && digit <= 'F') digit = static_cast<char>(digit + ('a' - 'A'));
        enforce(digest == member(entry, "sha256").as_string().view(),
            [&] { return concatenate("Embedded template hash mismatch: ",id.view()); }, 75);
        auto value = ngRigTemplate(static_cast<const compat::RetainedStringSlice&>(id));
        const auto version = member(value, "version").as_string();
        const auto expectedVersion = member(entry, "version").as_string();
        enforce(version.view() == expectedVersion.view(),
            [] { return compat::RetainedString::fromStaticLiteral("Embedded template version mismatch"); }, 77);
    }
}
} // namespace nijigenerate::autorig::deterministic::templates
