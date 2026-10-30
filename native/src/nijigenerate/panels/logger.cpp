// Copyright 2020-2023 Inochi2D Project, 2024 nijigenerate Project.
// Author: Luna Nielsen. Distributed under the 2-Clause BSD License.
#include "logger.hpp"
#include "nijigenerate/core/translation.hpp"

namespace nijigenerate::panels::logger {
LoggerPanel::LoggerPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Logger"),
        core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Logger")), false) {}

// The authored logger panel intentionally has no update contents.
void LoggerPanel::onUpdate() {}

void ngInitializeLoggerPanelModule() {
    ngInitializePanelModule<LoggerPanel>();
}
