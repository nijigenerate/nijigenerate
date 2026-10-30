#include "project.hpp"
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/windows/autosave.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <load_diagnostics.hpp>
#include <nijigenerate/compat/retained_exception_message.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::project {
namespace {
using core::translation::ngTranslate;
compat::RetainedString message(const std::exception& error) {
    return compat::ngRetainExceptionMessage(error);
}
std::string format(std::string_view text, core::translation::TypedFormatArgument argument) {
    return core::translation::ngFormatTyped(text, std::span(&argument, 1));
}
void appendDialogText(std::string& text, std::string_view suffix) {
    if (suffix.empty()) return;
    try {
        text.append(suffix);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString joinDialogText(std::string_view left, std::string_view right,
        std::string_view separator = "\n\n") {
    try {
        std::string text(left);
        text += separator;
        text += right;
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class Message> void show(const char* title, Message&& readMessage,
        widgets::dialog::DialogLevel level = widgets::dialog::DialogLevel::Error) {
    const auto name = core::translation::ngTranslateCString(title);
    const auto text = readMessage();
    widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(name.data(), name.owner),
        widgets::dialog::DialogText(text), level);
}
}
bool incOpenProject(compat::RetainedStringSlice path) {
    if (io::autosave::incCheckLockfile(path)) {
        windows::base::incPushWindow(windows::base::ngCreateWindow<windows::autosave::RestoreSaveWindow>(path));
        return false;
    }
    return incOpenProject(path, compat::RetainedString::emptyLiteral());
}
bool incOpenProject(compat::RetainedStringSlice mainPath, compat::RetainedStringSlice backupPath) {
    incClearImguiData();
    std::shared_ptr<Puppet> puppet;
    try {
        puppet = io::project_reader::ngLoadProjectPuppet<ext::ExPuppet>(backupPath.empty() ? mainPath : backupPath);
    } catch (const compat::FileException& error) {
        show("Error", [&] { return message(error); }); return false;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const compat::ThrowableError&) {
        throw;
    } catch (const std::exception& error) {
        std::string report;
        try {
            const auto* throwable = dynamic_cast<const compat::Throwable*>(&error);
            // Keep actual source exceptions intact; adapt only native backend exceptions.
            const auto path = [&] {
                const auto name = compat::RetainedString::fromStaticLiteral("nijigenerate-runtime-error");
                if (throwable) return utils::crashdump::writeCrashDump(name, *throwable);
                compat::ThrowableException nativeException(compat::RetainedString(message(error)));
                return utils::crashdump::writeCrashDump(name, nativeException);
            }();
            report = format(ngTranslate("Please report this file to the developers:\n\n%s"),
                core::translation::ngFormatArgument(path));
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const compat::ThrowableError&) {
            throw;
        } catch (const std::exception& dumpError) {
            const auto dumpMessage = message(dumpError);
            const auto failed = joinDialogText("Failed to write crash dump file.", dumpMessage.view(), "");
            report = ngTranslate(failed.view());
        }
        show("Error", [&] {
            const auto errorMessage = message(error);
            return joinDialogText(errorMessage.view(), report);
        }); return false;
    }
    incNewProject();
    compat::ngRequireObject(incActiveProject())->path = mainPath;
    incAddPrevProject(core::settings::SettingsString(mainPath));
    incResetRootNode(puppet);
    compat::ngRequireObject(incActiveProject())->puppet = puppet;
    compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->build();
    auto callbacks = loadCallbacks;
    for (const auto callback : callbacks) {
        if (!callback) compat::ngFailNullAccess(false);
        callback(puppet);
    }
    incFocusCamera(compat::ngRequireObject(incActivePuppet())->root);
    incInitAnimationPlayer(puppet);
    const auto statusPattern = ngTranslate("%s opened successfully.");
    const auto statusArgument = core::translation::ngFormatArgument(
        compat::ngRequireObject(incActiveProject())->path);
    core::tasks::ngSetStatus(format(statusPattern, statusArgument));
    const auto currentPath = compat::ngRequireObject(incActiveProject())->path;
    const auto basename = platform::ngAutosaveBaseName(currentPath.view());
    const auto offset = basename.data()
        ? static_cast<std::size_t>(basename.data() - currentPath.data()) : 0;
    const auto subtitle = basename.data()
        ? currentPath.slice(offset, offset + basename.size()) : compat::RetainedStringSlice{};
    core::ngSetWindowTitle(subtitle);
    const auto& diagnostics = nicxlive::core::serde::inLastLoadDiagnostics;
    if (diagnostics.recoveredComponents || diagnostics.repairedWeldingMappings) {
        core::actionstack::incActionInvalidateSavedState();
        std::string text;
        if (diagnostics.recoveredComponents)
            text = format(ngTranslate("Recovered %s nonfinite deformation components while loading this model. "
                "Missing components were reconstructed from nearby finite mesh samples; valid values were preserved. The original file was not changed."),
                core::translation::ngFormatArgument(diagnostics.recoveredComponents));
        if (diagnostics.repairedWeldingMappings) {
            if (!text.empty()) appendDialogText(text, "\n\n");
            const auto repaired = format(ngTranslate("Removed %s inconsistent Welding vertex mappings while loading this model. "
                "Reciprocal mappings were preserved. The original file was not changed."),
                core::translation::ngFormatArgument(diagnostics.repairedWeldingMappings));
            appendDialogText(text, repaired);
        }
        std::string examples;
        const auto warnings = diagnostics.warnings.slice(0,
            std::min<std::size_t>(3, diagnostics.warnings.size()));
        bool first = true;
        for (const auto& warning : warnings) {
            if (!first) appendDialogText(examples, "\n");
            first = false;
            appendDialogText(examples, warning);
        }
        show("Warning", [&] { return joinDialogText(text, examples); }, widgets::dialog::DialogLevel::Warning);
    }
    return true;
}
}
