#pragma once
// Used std.file DirIterator shallow/depth closure, D Language Foundation.
// BSL-1.0; see port/licenses/phobos-psd-file.txt.
#include "phobos_file_rmdir.hpp"
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

namespace nijigenerate::compat::phobos {
enum class SpanMode { shallow, depth };
namespace directory_range_detail { class IteratorState; }

// Copies retain the same actual cursor, as Source SafeRefCounted does.
class DirectoryEntryRange {
    using State = directory_range_detail::IteratorState;
    std::shared_ptr<State> state_;
    explicit DirectoryEntryRange(std::shared_ptr<State> state) : state_(std::move(state)) {}
    friend DirectoryEntryRange ngDirEntries(RetainedStringSlice, SpanMode, bool);
public:
    DirectoryEntryRange(const DirectoryEntryRange&) = default;
    DirectoryEntryRange& operator=(const DirectoryEntryRange&) = default;
    DirectoryEntryRange(DirectoryEntryRange&& other) noexcept : state_(other.state_) {}
    DirectoryEntryRange& operator=(DirectoryEntryRange&& other) noexcept {
        state_ = other.state_;
        return *this;
    }
    bool empty() const;
    DirectoryEntry front() const;
    void popFront();

    struct Sentinel {};
    class Iterator {
        std::shared_ptr<State> state_;
        explicit Iterator(std::shared_ptr<State> state) : state_(std::move(state)) {}
        friend class DirectoryEntryRange;
        bool atEnd() const;
    public:
        using value_type = DirectoryEntry;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;
        Iterator() = default;
        DirectoryEntry operator*() const;
        Iterator& operator++();
        void operator++(int) { ++*this; }
        friend bool operator==(const Iterator& iterator, Sentinel) { return iterator.atEnd(); }
    };
    Iterator begin() { return Iterator(state_); }
    Sentinel end() const { return {}; }
};

DirectoryEntryRange ngDirEntries(RetainedStringSlice path, SpanMode mode, bool followSymlink = true);
} // namespace nijigenerate::compat::phobos
