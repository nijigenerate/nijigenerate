#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <cstddef>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::widgets::dialog::detail {

// A port adapter must explicitly distinguish external lifetime from managed retention.
template<class T>
class RetainedPointer {
public:
    RetainedPointer() = default;
    RetainedPointer(const RetainedPointer&) = default;
    RetainedPointer& operator=(const RetainedPointer&) = default;

    static RetainedPointer borrowed(T* pointer) noexcept {
        RetainedPointer result;
        result.pointer_ = pointer;
        return result;
    }

    static RetainedPointer retained(T* pointer, std::shared_ptr<const void> owner) {
        if (pointer != nullptr && owner.use_count() == 0) {
            throw std::invalid_argument("A managed dialog pointer requires an owner");
        }
        RetainedPointer result;
        result.owner_ = std::move(owner);
        result.pointer_ = pointer;
        return result;
    }

    T* get() const noexcept { return pointer_; }
    bool isNull() const noexcept { return pointer_ == nullptr; }

    friend bool operator==(const RetainedPointer& left, const RetainedPointer& right) noexcept {
        return left.pointer_ == right.pointer_;
    }

private:
    std::shared_ptr<const void> owner_;
    T* pointer_ = nullptr;
};

// Copying, including an rvalue copy, preserves immutable backing and pointer identity.
class DialogText {
public:
    DialogText() = default;
    DialogText(const DialogText&) = default;
    DialogText& operator=(const DialogText&) = default;
    DialogText(compat::RetainedString text) noexcept : text_(std::move(text)) {}

    static DialogText borrowed(std::string_view text) noexcept {
        return DialogText(compat::RetainedString::fromBorrowedView(text));
    }

    static DialogText retained(std::string_view text, std::shared_ptr<const void> owner) {
        if (text.data() != nullptr && owner.use_count() == 0) {
            throw std::invalid_argument("Managed dialog text requires an owner");
        }
        if (!text.data()) return {};
        // Alias the same actual owner to its real bytes; a null alias may still
        // own a control block, so do not mistake its pointer value for lifetime.
        auto bytesOwner = std::shared_ptr<const void>(std::move(owner), text.data());
        return DialogText(compat::RetainedString::retainedView(
            std::move(bytesOwner), text.data(), text.size()));
    }

    static DialogText owned(std::string text) {
        try {
            return DialogText(compat::RetainedString(std::move(text)));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }

    const char* data() const noexcept { return text_.data(); }
    std::size_t size() const noexcept { return text_.size(); }
    bool isNull() const noexcept { return text_.isNull(); }
    std::string_view view() const noexcept { return text_.view(); }
    compat::RetainedString retained() const noexcept { return text_; }

    DialogText slice(std::size_t begin, std::size_t end) const {
        if (begin > end || end > size()) {
            throw std::out_of_range("Dialog text slice bounds");
        }
        if (isNull()) return *this;
        return DialogText(text_.subslice(begin, end));
    }

private:
    compat::RetainedString text_;
};

} // namespace nijigenerate::widgets::dialog::detail
