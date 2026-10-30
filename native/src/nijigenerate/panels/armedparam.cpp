// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "armedparam.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include "parameters.hpp"

namespace nijigenerate::panels::armedparam {
void ArmedParametersPanel::onUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    parameters::incRunPendingParameterUiCommand();
    const auto puppet = project::incActivePuppet();
    auto values = compat::ngRequireObject(puppet)->parameters;
    if (project::incArmedParameter()) {
        const auto index = project::incArmedParameterIdx();
        const auto parameter = project::incArmedParameter();
        parameters::incParameterView<true>(index, parameter, &grabParam, false, values);
    }
}

ArmedParametersPanel::ArmedParametersPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Armed Parameters"),
        core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Armed Parameters")), true) {}

bool ArmedParametersPanel::isActive() {
    return project::incArmedParameter() != nullptr;
}
void ngInitializeArmedParametersPanel() {
    panels::ngInitializePanelModule<ArmedParametersPanel>();
}

} // namespace nijigenerate::panels::armedparam
