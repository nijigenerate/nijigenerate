#include "phobos_file_rmdir.hpp"
#include "phobos_file_directory_range.hpp"
#include "phobos_file_exists.hpp"
#include "phobos_build_path.hpp"
#include "phobos_temp_cstring.hpp"
#include "phobos_file_exception.hpp"
#include "slice.hpp"
#include "core_time_exception.hpp"
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#if defined(_WIN32)
#include <cwchar>
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace nijigenerate::compat::phobos {
namespace {
using String = RetainedStringSlice;
using Entry = platform::AutosaveFileEntry;
[[noreturn]] void fail(String name, std::uint32_t code, std::size_t line) {
    FileException error(name.retainedString(), code);
    error.file = RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
template<std::size_t N> [[noreturn]] void fail(String name, const char (&message)[N], std::size_t line) {
    FileException error(name.retainedString(), RetainedString::fromStaticLiteral(message));
    error.file = RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
#ifdef _WIN32
RetainedString narrowName(std::wstring_view name) {
    detail::UnicodeCursor<std::span<const wchar_t>, wchar_t> cursor(
        std::span<const wchar_t>{name.data(), name.size()});
    std::string bytes;
    while (!cursor.empty()) {
        const auto scalar = cursor.front();
        platform::detail::ngPathAllocation([&] { file_exception_detail::appendScalar(bytes, scalar); });
        cursor.popFront();
    }
    return platform::detail::ngPathAllocation([&] { return RetainedString(std::move(bytes)); });
}
std::int64_t instant(FILETIME time) {
    constexpr std::int64_t epoch = 504911232000000000;
    const auto ticks = (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    if (ticks > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - epoch))
        throw TimeException(RetainedString::fromStaticLiteral(
            "The given FILETIME cannot be represented as a stdTime value."),
            RetainedString::fromStaticLiteral("std/datetime/systime.d"), 10134);
    return static_cast<std::int64_t>(ticks) + epoch;
}
template<class Data> void snapshot(Entry& entry, const Data& data) {
    entry.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    entry.created = instant(data.ftCreationTime);
    entry.accessed = instant(data.ftLastAccessTime);
    entry.modified = instant(data.ftLastWriteTime);
    entry.attributes = data.dwFileAttributes;
    entry.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry.symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
#endif
DirectoryEntry fromPath(String path) {
    if (!ngFileExists(path.view())) fail(path, "File does not exist",
#ifdef _WIN32
        3820
#else
        3927
#endif
    );
    Entry entry;
    entry.name = path;
#ifdef _WIN32
    const TempCStringBuffer<wchar_t> name(path.view());
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(name.c_str(), GetFileExInfoStandard, &data)) {
        // The lazy name.idup precedes FileException's default GetLastError.
        const auto copied = platform::detail::ngPathAllocation([&] { return RetainedString(path.view()); });
        fail(copied, GetLastError(), 1122);
    }
    snapshot(entry, data);
#endif
    return DirectoryEntry(std::move(entry));
}
bool isDirectory(std::uint32_t attributes) {
#ifdef _WIN32
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return (attributes & S_IFMT) == S_IFDIR;
#endif
}
void removeDirectory(String path) {
#ifdef _WIN32
    const TempCStringBuffer<wchar_t> name(path.view());
    if (!RemoveDirectoryW(name.c_str())) {
        const auto diagnostic = path.empty()
            ? narrowName(std::wstring_view(name.c_str() ? name.c_str() : L"")) : path.retainedString();
        fail(diagnostic, GetLastError(), 3182);
    }
#else
    const TempCStringBuffer<char> name(path.view());
    if (::rmdir(name.c_str()) != 0) {
        const auto diagnostic = path.empty() ? platform::detail::ngPathAllocation([&] {
            return RetainedString(std::string_view(name.c_str() ? name.c_str() : ""));
        }) : path.retainedString();
        fail(diagnostic, static_cast<std::uint32_t>(errno), 3182);
    }
#endif
}

} // namespace

namespace directory_range_detail {
// The same Source DirIteratorImpl carries both the general used range and
// rmdir's depth/no-follow closure. Neither path collects a filesystem snapshot.
class IteratorState {
    SpanMode mode_ = SpanMode::shallow;
    bool followSymlink_ = true;
    struct Handle {
        String path;
#ifdef _WIN32
        HANDLE value{};
#else
        DIR* value{};
#endif
    };
    Slice<Handle> stack_;
    Slice<DirectoryEntry> stashed_;
    DirectoryEntry current_;
#ifdef _WIN32
    WIN32_FIND_DATAW find_;
#endif
    void popDirectory() {
#ifdef _WIN32
        FindClose(stack_[stack_.size() - 1].value);
#else
        ::closedir(stack_[stack_.size() - 1].value);
#endif
        stack_.resize(stack_.size() - 1);
    }
#ifdef _WIN32
    bool toNext(bool fetch) {
        const auto handle = stack_[stack_.size() - 1].value;
        if (fetch && !FindNextFileW(handle, &find_)) { popDirectory(); return false; }
        while (!std::wcscmp(find_.cFileName, L".") || !std::wcscmp(find_.cFileName, L".."))
            if (!FindNextFileW(handle, &find_)) { popDirectory(); return false; }
        find_.cFileName[std::size(find_.cFileName) - 1] = 0;
        const auto filename = narrowName(std::wstring_view(find_.cFileName, std::wcslen(find_.cFileName)));
        const std::array segments{stack_[stack_.size() - 1].path.retainedString(), filename};
        Entry entry;
        entry.name = ngBuildPath(segments);
        snapshot(entry, find_);
        current_ = DirectoryEntry(std::move(entry));
        return true;
    }
#endif
    bool next() {
        if (stack_.empty()) return false;
#ifdef _WIN32
        return toNext(true);
#else
        while (const auto item = ::readdir(stack_[stack_.size() - 1].value)) {
            if (!std::strcmp(item->d_name, ".") || !std::strcmp(item->d_name, "..")) continue;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
            const std::string_view filename(item->d_name, item->d_namlen);
#else
            const std::string_view filename(item->d_name, std::strlen(item->d_name));
#endif
            const std::array segments{stack_[stack_.size() - 1].path.retainedString(),
                RetainedString::fromBorrowedView(filename)};
            Entry entry;
            entry.name = ngBuildPath(segments);
            current_ = DirectoryEntry(std::move(entry));
            return true;
        }
        popDirectory();
        return false;
#endif
    }
    bool stepIn(String path) {
#ifdef _WIN32
        const std::array segments{path.retainedString(), RetainedString::fromStaticLiteral("*.*")};
        const auto pattern = ngBuildPath(segments);
        const auto handle = [&] {
            const TempCStringBuffer<wchar_t> name(pattern.view());
            return FindFirstFileW(name.c_str(), &find_);
        }();
        if (handle == INVALID_HANDLE_VALUE) fail(path, GetLastError(), 4683);
        stack_.append(Handle{path, handle});
        return toNext(false);
#else
        const auto handle = [&] {
            const TempCStringBuffer<char> name(path.empty() ? std::string_view{"."} : path.view());
            return ::opendir(name.c_str());
        }();
        if (!handle) fail(path, static_cast<std::uint32_t>(errno), 4752);
        stack_.append(Handle{path, handle});
        return next();
#endif
    }
    bool mayStepIn() {
#ifdef _WIN32
        return followSymlink_ ? current_.isDir() : current_.isDir() && !current_.isSymlink();
#else
        return followSymlink_ ? current_.isDir() : isDirectory(current_.linkAttributes());
#endif
    }
    void descend() {
        while (mayStepIn()) {
            const auto directory = current_;
            if (stepIn(current_.name())) stashed_.append(directory);
            else break;
        }
    }
public:
    void initialize(String path, SpanMode mode, bool followSymlink) {
        mode_ = mode;
        followSymlink_ = followSymlink;
        if (stepIn(path) && mode_ == SpanMode::depth) descend();
    }
    IteratorState() = default;
    IteratorState(const IteratorState&) = delete;
    ~IteratorState() {
        // Source releaseDirStack visits remaining handles from oldest to newest.
        for (const auto handle : stack_) {
#ifdef _WIN32
            FindClose(handle.value);
#else
            ::closedir(handle.value);
#endif
        }
    }
    bool empty() const { return stashed_.empty() && stack_.empty(); }
    DirectoryEntry front() const { return current_; }
    void popFront() {
        if (mode_ != SpanMode::depth) { next(); return; }
        if (next()) descend();
        else if (!stashed_.empty()) {
            current_ = stashed_[stashed_.size() - 1];
            stashed_.resize(stashed_.size() - 1);
        }
    }
};
} // namespace directory_range_detail

DirectoryEntryRange ngDirEntries(String path, SpanMode mode, bool followSymlink) {
    using State = directory_range_detail::IteratorState;
    auto state = std::allocate_shared<State>(nicxlive::core::d_runtime::SourceAllocator<State>{});
    state->initialize(path, mode, followSymlink);
    return DirectoryEntryRange(std::move(state));
}
bool DirectoryEntryRange::empty() const { return state_->empty(); }
DirectoryEntry DirectoryEntryRange::front() const { return state_->front(); }
void DirectoryEntryRange::popFront() { state_->popFront(); }
bool DirectoryEntryRange::Iterator::atEnd() const { return !state_ || state_->empty(); }
DirectoryEntry DirectoryEntryRange::Iterator::operator*() const { return state_->front(); }
DirectoryEntryRange::Iterator& DirectoryEntryRange::Iterator::operator++() {
    state_->popFront();
    return *this;
}

void ngRmdirRecurse(DirectoryEntry& entry) {
    if (!entry.isDir()) fail(entry.name(), "Not a directory", 4466);
    if (entry.isSymlink()) {
#ifdef _WIN32
        removeDirectory(entry.name());
#else
        platform::ngAutosaveRemove(entry.name());
#endif
    } else {
        {
            auto iterator = ngDirEntries(entry.name(), SpanMode::depth, false);
            while (!iterator.empty()) {
                auto child = iterator.front();
                if (isDirectory(child.linkAttributes())) removeDirectory(child.name());
                else platform::ngAutosaveRemove(child.name());
                iterator.popFront();
            }
        }
        removeDirectory(entry.name());
    }
}
void ngRmdirRecurse(String path) {
    auto entry = fromPath(path);
    ngRmdirRecurse(entry);
}
void ngRmdirRecurse(DirectoryEntry&& entry) { ngRmdirRecurse(entry); }
} // namespace nijigenerate::compat::phobos
