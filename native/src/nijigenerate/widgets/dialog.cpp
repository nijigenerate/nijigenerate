#include "nijigenerate/widgets/dialog.hpp"
#include "nijigenerate/widgets/detail/dialog_state.hpp"

#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets::dialog {

namespace detail {
thread_local compat::Slice<DialogEntry> entries;
thread_local compat::Slice<DialogHandlerRef> dialogHandlers;
thread_local compat::Slice<std::shared_ptr<nicxlive::core::Texture>> adaTextures;

DialogEntryReference findDialogEntry(const char* tag) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].tag.get() == tag) {
            return DialogEntryReference(entries, i);
        }
    }
    return {};
}
} // namespace detail

void ngDialog(DialogTag tag, DialogTag title, DialogText body,
              DialogLevel level, DialogButtons buttons, DialogUserData userData) {
    std::int32_t count = 0;
    if ((buttons & DialogButtons::OK) == DialogButtons::OK) ++count;
    if ((buttons & DialogButtons::Cancel) == DialogButtons::Cancel) ++count;
    if ((buttons & DialogButtons::Yes) == DialogButtons::Yes) ++count;
    if ((buttons & DialogButtons::No) == DialogButtons::No) ++count;
    detail::entries.append(detail::DialogEntry{tag, title, body, level, buttons,
                                             DialogButtons::NONE, count, userData});
}

void ngDialog(DialogTag title, DialogText body, DialogLevel level,
              DialogButtons buttons, DialogUserData userData) {
    ngDialog(title, title, body, level, buttons, userData);
}

void ngCleanupDialogs() {
    auto& entries = detail::entries;
    if (!entries.empty() && static_cast<std::int32_t>(entries[0].selected) > 0) {
        entries = entries.slice(1, entries.size());
    }
}

DialogButtons ngDialogButtonSelected(const char* tag) {
    const auto& entries = detail::entries;
    if (entries.empty()) return DialogButtons::NONE;
    if (entries[0].tag.get() != tag) return DialogButtons::NONE;
    return entries[0].selected;
}

DialogUserData ngDialogButtonUserData(const char* tag) {
    const auto& entries = detail::entries;
    if (entries.empty()) return {};
    if (entries[0].tag.get() != tag) return {};
    return entries[0].userData;
}

DialogHandler::DialogHandler(DialogTag tag) : tag_(tag) {}

bool DialogHandler::hasClicked() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return ngDialogButtonSelected(tag_.get()) != DialogButtons::NONE;
}

bool DialogHandler::onClick(DialogButtons button) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    switch (button) {
    case DialogButtons::Cancel: return onClickCancel();
    case DialogButtons::Yes: return onClickYes();
    case DialogButtons::No: return onClickNo();
    default: throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Invalid button"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/dialog.d"), 237);
    }
}

bool DialogHandler::onClickCancel() { return false; }
bool DialogHandler::onClickYes() { return false; }
bool DialogHandler::onClickNo() { return false; }

void DialogHandler::registerHandler() {
    ngRegisterDialogHandler(shared_from_this());
}

void ngRegisterDialogHandler(DialogHandlerRef handler) {
    detail::dialogHandlers.append(handler);
}

void ngHandleDialogHandlers() {
    const auto active = detail::dialogHandlers;
    for (std::size_t i = 0; i < active.size(); ++i) {
        const auto handler = active[i];
        if (!compat::ngRequireObject(handler)->hasClicked()) continue;
        compat::ngRequireObject(handler)->onClick(ngDialogButtonSelected(handler->tag_.get()));
    }

    const auto filtering = detail::dialogHandlers;
    compat::Slice<DialogHandlerRef> retained;
    for (std::size_t i = 0; i < filtering.size(); ++i) {
        bool keep;
        {
            const auto handler = filtering[i];
            keep = !compat::ngRequireObject(handler)->hasClicked();
        }
        // Phobos filter primes using the predicate, then array reads the
        // actual current front cell, rather than the predicate's local value.
        if (keep) retained.append(filtering[i]);
    }
    detail::dialogHandlers = retained;
}

// object.Object defaults (Digital Mars 2000-2011), BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DialogHandler::prepareSourceClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<DialogHandler>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<DialogHandler>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(DialogHandler), name);
        return true;
    }();
    (void)registered;
}
compat::RetainedString DialogHandler::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DialogHandler::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DialogHandler::opEquals(DialogHandlerRef other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DialogHandler::opCmp(DialogHandlerRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDialogHandlerObjectEquals(DialogHandlerRef left, DialogHandlerRef right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

} // namespace nijigenerate::widgets::dialog
