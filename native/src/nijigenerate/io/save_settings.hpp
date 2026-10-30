#pragma once
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/slice.hpp>
namespace nijigenerate::io::save {
using SaveOptions = core::settings::SettingsStringMap;
inline SaveOptions incGetSaveProjectOption() {
    const auto ask = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Always ask"));
    const auto dontSave = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Don't save"));
    return SaveOptions{{compat::RetainedString::fromStaticLiteral("Ask"), ask},
        {compat::RetainedString::fromStaticLiteral("dontSave"), dontSave}};
}
inline void incSetSaveProjectOnClose(core::settings::SettingsString value) {
    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("SaveProjectOnClose"), value);
}
inline core::settings::SettingsString incGetSaveProjectOnClose() {
    auto config = core::settings::incSettingsGet<core::settings::SettingsString>(
        compat::RetainedString::fromStaticLiteral("SaveProjectOnClose"),
        compat::RetainedString::fromStaticLiteral("Ask"));
    const auto options = incGetSaveProjectOption();
    compat::Slice<core::settings::SettingsString> keys;
    keys.resize(options.size());
    std::size_t index = 0;
    for (const auto& entry : options) keys[index++] = entry.first;
    bool found = false;
    for (const auto& key : keys) {
        if (key == config) { found = true; break; }
    }
    if (!found) {
        config = compat::RetainedString::fromStaticLiteral("Ask");
        incSetSaveProjectOnClose(config);
    }
    return config;
}
}
