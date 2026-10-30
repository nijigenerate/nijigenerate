#include "project.hpp"
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/retained_exception_message.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_file_rename.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::project {
namespace {
template<class Operation> decltype(auto) allocateSavePathMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class PuppetType> void writeProject(PuppetType& puppet, compat::RetainedStringSlice path) {
    const auto bytes = io::ngWriteProjectPuppetMemory(puppet);
    const auto buffer = bytes.empty() ? std::string_view{}
        : std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    compat::phobos::ngFileWrite(path, buffer);
}
void replaceFile(compat::RetainedStringSlice from, compat::RetainedStringSlice to) {
    compat::phobos::ngFileRename(from, to);
}
template<std::size_t N> compat::RetainedString formatPath(const char (&pattern)[N]) {
    const auto translated = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(pattern));
    const core::translation::TypedFormatArgument args[] = {
        compat::ngRequireObject(incActiveProject())->path};
    auto formatted = core::translation::ngFormatTyped(translated.view(), args);
    return allocateSavePathMemory([&] { return compat::RetainedString(std::move(formatted)); });
}
void postSave() {
    const auto callbacks = postSaveCallbacks;
    for (auto callback : callbacks) {
        if (!callback) compat::ngFailNullAccess(false);
        callback(incActivePuppet());
    }
}
}
bool incSaveProject(compat::RetainedStringSlice path, compat::RetainedStringSlice autosaveStamp) {
    try {
        const bool isAutosave = !autosaveStamp.empty();
        const auto finalPath = [&] {
            if (isAutosave) return allocateSavePathMemory([&] {
                return compat::RetainedString(
                    std::string(path.view()) + "_" + std::string(autosaveStamp.view()) + ".inx");
            });
            const auto stem = platform::ngAutosaveStripExtension(path.view());
            return allocateSavePathMemory([&] {
                return compat::RetainedString(std::string(stem) + ".inx");
            });
        }();
        compat::ngRequireObject(incActivePuppet())->populateTextureSlots();
        compat::ngRunWithScopeExit([&] {
            const auto callbacks = saveCallbacks;
            for (auto callback : callbacks) {
                if (!callback) compat::ngFailNullAccess(false);
                callback(incActivePuppet());
            }
            ngPruneProjectAnimationLanes(incActivePuppet());
            const auto swapPath = allocateSavePathMemory([&] {
                return compat::RetainedString(std::string(finalPath.view()) + ".swp");
            });
            writeProject(*compat::ngRequireObject(incActivePuppet()), swapPath);
            replaceFile(swapPath, finalPath);
            if (isAutosave) io::autosave::incAddPrevAutosave(finalPath);
            else {
                compat::ngRequireObject(incActiveProject())->path = path;
                incAddPrevProject(finalPath);
            }
            if (!isAutosave) core::actionstack::incActionMarkSaved();
            if (!isAutosave) io::autosave::incReleaseLockfile();
            if (!compat::ngRequireObject(incActivePuppet())->getDrivers().empty())
                compat::ngRequireObject(incActivePuppet())->resetDrivers();
            core::tasks::ngSetStatus(compat::RetainedStringSlice(formatPath("%s saved successfully.")));
            const auto currentPath = compat::ngRequireObject(incActiveProject())->path;
            const auto basename = platform::ngAutosaveBaseName(currentPath.view());
            const auto offset = basename.data()
                ? static_cast<std::size_t>(basename.data() - currentPath.data()) : 0;
            const auto subtitle = basename.data()
                ? currentPath.slice(offset, offset + basename.size()) : compat::RetainedStringSlice{};
            core::ngSetWindowTitle(subtitle);
        }, postSave);
        return true;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        core::tasks::ngSetStatus(compat::RetainedStringSlice(formatPath("Failed to save %s")));
        const auto title = core::translation::ngTranslateCString("Error");
        const auto message = compat::ngRetainExceptionMessage(error);
        widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(title.data(), title.owner),
            widgets::dialog::DialogText(message));
        return false;
    }
}
}

namespace nijigenerate::io {
void ngWriteProjectPuppetFile(const nicxlive::core::Puppet& puppet, compat::RetainedStringSlice path) {
    project::writeProject(puppet, path);
}
void ngRenameProjectFile(compat::RetainedStringSlice from, compat::RetainedStringSlice to) {
    project::replaceFile(from, to);
}
}
