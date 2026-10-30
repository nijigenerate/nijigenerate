#include "project.hpp"
#include "project_animation_detail.hpp"
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/io/imagefmt/reader.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/retained_exception_message.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/core/colorbleed.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/i18n_case.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/windows/inpexport.hpp>
#include <nodes/part.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <cwchar>
#include <iterator>
#include <concepts>
#include <type_traits>
#include <span>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <cstring>
#include <dirent.h>
#endif

namespace nijigenerate::project {
using core::translation::ngTranslate;
namespace {
template<class Operation> decltype(auto) allocateImportPathMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
#if defined(_WIN32)
std::wstring wide(std::string_view text) {
    std::wstring result;
    core::i18n::detail::LocaleCaseCursor cursor{text};
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        allocateImportPathMemory([&] {
            if (scalar <= 0xffff) result.push_back(static_cast<wchar_t>(scalar));
            else {
                scalar -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10)));
                result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff)));
            }
        });
    }
    return result;
}
std::string utf8(const wchar_t* text) {
    using Cursor = compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
    Cursor cursor(std::span<const wchar_t>{text, std::wcslen(text)});
    std::string result;
    while (!cursor.empty()) {
        const auto scalar = cursor.front();
        allocateImportPathMemory([&] { compat::file_exception_detail::appendScalar(result, scalar); });
        cursor.popFront();
    }
    return result;
}
#endif
// Used shallow std.file directory iterator: Copyright The D Language Foundation 2007-.
// BSL-1.0; see port/licenses/phobos-psd-file.txt.
template<class Worker> void directoryEntries(std::string_view folder, Worker worker) {
#if defined(_WIN32)
    const auto prefix = allocateImportPathMemory([&] {
        auto bytes = std::string(folder);
        if (!bytes.empty() && bytes.back() != '/' && bytes.back() != '\\') bytes += '\\';
        return bytes;
    });
    const auto patternText = allocateImportPathMemory([&] { return prefix + "*.*"; });
    const auto pattern = wide(patternText);
    WIN32_FIND_DATAW data{};
    const auto handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto code = GetLastError();
        const auto errorPath = allocateImportPathMemory([&] { return compat::RetainedString(folder); });
        compat::phobos::directory_detail::fileError(errorPath, code, 4683);
    }
    struct Guard { HANDLE handle; ~Guard() { FindClose(handle); } } guard{handle};
    do {
        if (std::wcscmp(data.cFileName, L".") == 0 || std::wcscmp(data.cFileName, L"..") == 0) continue;
        data.cFileName[std::size(data.cFileName) - 1] = L'\0';
        const auto fileName = utf8(data.cFileName);
        const auto name = allocateImportPathMemory([&] { return compat::RetainedString(prefix + fileName); });
        worker(compat::RetainedStringSlice(name));
    } while (FindNextFileW(handle, &data));
#else
    const auto directory = allocateImportPathMemory([&] { return std::string(folder); });
    auto* handle = opendir(directory.empty() ? "." : directory.c_str());
    if (!handle) {
        const auto code = errno;
        const auto errorPath = allocateImportPathMemory([&] { return compat::RetainedString(directory); });
        compat::phobos::directory_detail::fileError(errorPath, static_cast<std::uint32_t>(code), 4752);
    }
    struct Guard { DIR* handle; ~Guard() { closedir(handle); } } guard{handle};
    while (const auto* entry = readdir(handle)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
        // Only DirEntry.name is consumed; shallow iteration does not stat or follow entries.
        const std::array<compat::RetainedString, 2> segments{
            compat::RetainedString::retainedView({}, directory.data(), directory.size()),
            compat::RetainedString::retainedView({}, entry->d_name, std::strlen(entry->d_name))};
        const auto name = compat::phobos::ngBuildPath(segments);
        worker(compat::RetainedStringSlice(name));
    }
#endif
}
std::string format(std::string_view pattern, std::string_view value) {
    const auto argument = core::translation::ngFormatArgument(value);
    return core::translation::ngFormatTyped(pattern, std::span(&argument, 1));
}
template<class Message> void dialog(const char* title, Message&& readMessage, bool translateTitle) {
    using namespace widgets::dialog;
    const auto tag = [&] {
        if (!translateTitle) return DialogTag::borrowed(title);
        const auto name = core::translation::ngTranslateCString(title);
        return DialogTag::retained(name.data(), name.owner);
    }();
    auto text = readMessage();
    const auto body = [&] {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(text)>, compat::RetainedString>)
            return DialogText(text);
        else return DialogText::owned(std::move(text));
    }();
    ngDialog(tag, body);
}
void activateRestoredPuppet(std::shared_ptr<Puppet> puppet) {
    core::asyncderivedupdate::incAsyncDerivedUpdateClearScope(
        compat::ngRequireObject(incActiveProject())->derivedUpdateScope);
    incSelectNode(nullptr); incDisarmParameter();
    compat::ngRequireObject(incActiveProject())->puppet = puppet;
    compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->build();
    const auto callbacks = loadCallbacks;
    for (auto function : callbacks) {
        if (!function) compat::ngFailNullAccess(false);
        function(puppet);
    }
    incInitAnimationPlayer(puppet);
}
class PuppetMemoryRestoreAction : public actions::Action {
public:
    std::shared_ptr<Puppet> before, after;
    PuppetMemoryRestoreAction(const PuppetMemoryRestoreAction&) = delete;
    PuppetMemoryRestoreAction& operator=(const PuppetMemoryRestoreAction&) = delete;
    PuppetMemoryRestoreAction(PuppetMemoryRestoreAction&&) = delete;
    PuppetMemoryRestoreAction& operator=(PuppetMemoryRestoreAction&&) = delete;
    PuppetMemoryRestoreAction() = default;
    void initialize(std::shared_ptr<Puppet> before, std::shared_ptr<Puppet> after) {
        this->before = std::move(before);
        this->after = std::move(after);
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PuppetMemoryRestoreAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.project.PuppetMemoryRestoreAction"));
    }
    void rollback() override { activateRestoredPuppet(before); }
    void redo() override { activateRestoredPuppet(after); }
    virtual compat::RetainedString toString() {
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    actions::ActionText getName() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Restore model checkpoint"));
    }
    actions::ActionText describe() override { return getName(); }
    actions::ActionText describeUndo() override { return getName(); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
};
}
} // namespace nijigenerate::project

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::project::PuppetMemoryRestoreAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.project.PuppetMemoryRestoreAction)";
    inline static constexpr char unqualifiedArgument[] = "(PuppetMemoryRestoreAction)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.project.PuppetMemoryRestoreAction";
    inline static constexpr char unqualifiedElement[] = "PuppetMemoryRestoreAction";
};
template<> struct SourceObjectTypeNames<::nijigenerate::project::PuppetMemoryRestoreAction> {
    inline static constexpr char className[] = "nijigenerate.project.PuppetMemoryRestoreAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.project.PuppetMemoryRestoreAction)";
};
}

namespace nijigenerate::project {
void incImportFolder(compat::RetainedStringSlice folder) {
    incNewProject();
    compat::Slice<compat::RetainedString> failures;
    auto puppet = ext::ExPuppet::create();
    std::size_t index = 0;
    directoryEntries(folder.view(), [&](compat::RetainedStringSlice file) {
        try {
            nicxlive::core::ShallowTexture texture{file.retainedString()};
            nicxlive::core::inTexPremultiply(texture.data);
            const auto uploadedTexture = Texture::createSource(texture);
            const auto name = platform::ngAutosaveStripExtension(platform::ngAutosaveBaseName(file.view()));
            const auto offset = name.data() ? static_cast<std::size_t>(name.data() - file.data()) : 0;
            auto part = nicxlive::core::nodes::Part::createSimpleRetained(uploadedTexture,
                file.slice(offset, offset + name.size()).retainedString().toNullableString());
            compat::ngRequireObject(part)->zSort(-(static_cast<float>(index++) / 100));
            compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->addChild(part);
        } catch (const std::bad_alloc&) { throw; }
          catch (const compat::ThrowableError&) { throw; }
          catch (const std::exception& error) { failures.append(compat::ngRetainExceptionMessage(error)); }
    });
    if (!failures.empty()) {
        dialog("ImgLoadError", [&] {
            const auto pattern = core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("The following errors occured during file loading\n%s"));
            std::string joined;
            for (std::size_t i = 0; i < failures.size(); ++i) {
                if (i) allocateImportPathMemory([&] { joined += '\n'; });
                const auto bytes = failures[i].view();
                if (!bytes.empty()) allocateImportPathMemory([&] { joined.append(bytes); });
            }
            return format(pattern.view(), joined);
        }, false);
    }
    compat::ngRequireObject(puppet)->rescanNodes();
    compat::ngRequireObject(puppet)->populateTextureSlots();
    compat::ngRequireObject(incActiveProject())->puppet = puppet;
    incInitAnimationPlayer(puppet);
    incFocusCamera(compat::ngRequireObject(incActivePuppet())->root);
    const auto status = core::translation::ngTranslateRetained(failures.empty()
        ? compat::RetainedString::fromStaticLiteral("Folder import completed...")
        : compat::RetainedString::fromStaticLiteral("Folder import completed with errors..."));
    core::tasks::ngSetStatus(compat::RetainedStringSlice(status));
}
void incImportINP(compat::RetainedStringSlice file) {
    incNewProject();
    std::shared_ptr<Puppet> puppet;
    try {
        puppet = io::project_reader::ngLoadProjectPuppet<ext::ExPuppet>(file);
        // Original code formats first, then translates the resulting message.
        auto formatted = format("%s was imported...", file.view());
        const auto untranslated = allocateImportPathMemory([&] {
            return compat::RetainedString(std::move(formatted));
        });
        const auto status = core::translation::ngTranslateRetained(untranslated);
        core::tasks::ngSetStatus(compat::RetainedStringSlice(status));
        const auto basename = platform::ngAutosaveBaseName(file.view());
        const auto offset = basename.data()
            ? static_cast<std::size_t>(basename.data() - file.data()) : 0;
        const auto subtitle = basename.data()
            ? file.slice(offset, offset + basename.size()) : compat::RetainedStringSlice{};
        core::ngSetWindowTitle(subtitle);
    } catch (const std::bad_alloc&) { throw; }
      catch (const compat::ThrowableError&) { throw; }
      catch (const std::exception& error) {
        dialog("Error", [&] { return compat::ngRetainExceptionMessage(error); }, true);
        const auto status = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Import failed..."));
        core::tasks::ngSetStatus(compat::RetainedStringSlice(status)); return;
    }
    compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->build();
    compat::ngRequireObject(incActiveProject())->puppet = puppet;
    detail::installAnimationPlayer(core::animation::AnimationPlayer::create(puppet));
    incFocusCamera(compat::ngRequireObject(incActivePuppet())->root);
}
void ngRestorePuppetMemory(compat::Slice<std::uint8_t> data) {
    if (!(core::thread_context::ngHasManagedThread() && core::thread_context::ngIsMainThread()))
        throw compat::ThrowableException(detail::ngDuplicateProjectText(
            "Model restoration requires the main thread"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/project.d"), 541);
    const bool modified = core::actionstack::incActionIsModified();
    auto previous = incActivePuppet();
    bool unchanged = false;
    if (!modified) {
        const auto written = io::ngWriteProjectPuppetMemory(*previous);
        unchanged = written.size() == data.size() && std::equal(written.begin(), written.end(), data.begin());
    }
    auto restored = io::project_reader::ngLoadProjectInp<ext::ExPuppet>(data);
    if (!restored) throw compat::ThrowableException(
        detail::ngDuplicateProjectText("Could not restore model snapshot"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/project.d"), 547);
    auto action = detail::ngCreateProjectAction<PuppetMemoryRestoreAction>([&] {
        return std::tuple{previous, restored};
    });
    action->redo(); core::actionstack::incActionPush(action);
    if (unchanged) core::actionstack::incActionMarkSaved();
}
void incRegenerateMipmaps() {
    const auto slots = compat::ngRequireObject(compat::ngRequireObject(incActiveProject())->puppet)->textureSlots;
    for (const auto texture : slots) {
        compat::ngRequireObject(texture)->genMipmap();
        compat::ngRequireObject(texture)->setFiltering(nicxlive::core::Filtering::Linear);
    }
    core::tasks::ngSetStatus(ngTranslate("Mipmap generation completed."));
}
void incExportINP(compat::RetainedStringSlice file) {
    auto path = std::string(platform::ngAutosaveStripExtension(file.view())) + ".inp";
    windows::base::incPushWindow(windows::base::ngCreateWindow<windows::inpexport::ExportWindow>(
        compat::RetainedString(std::move(path))));
}
void incRebleedTextures() {
    core::tasks::ngTaskAddSource(compat::RetainedString::fromStaticLiteral("Rebleed"), [] {
        core::tasks::ngTaskStatusSource(compat::RetainedString::fromStaticLiteral("Bleeding textures..."));
        const auto slots = compat::ngRequireObject(compat::ngRequireObject(incActiveProject())->puppet)->textureSlots;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            auto texture = slots[i];
            core::tasks::ngTaskProgress(static_cast<float>(i)
                / compat::ngRequireObject(compat::ngRequireObject(incActiveProject())->puppet)->textureSlots.size());
            core::tasks::ngTaskYield();
            core::colorbleed::incColorBleedPixels(texture);
        }
    });
    core::tasks::ngSetStatus(ngTranslate("Texture bleeding completed."));
}
}
