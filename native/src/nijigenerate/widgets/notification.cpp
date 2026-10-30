#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <string>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {

thread_local std::shared_ptr<NotificationPopup> NotificationPopup::instance_{};

NotificationPopup::NotificationPopup() = default;
NotificationPopup::~NotificationPopup() = default;

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void NotificationPopup::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NotificationPopup),
            compat::RetainedString::fromStaticLiteral(
                nicxlive::core::d_runtime::SourceObjectTypeNames<NotificationPopup>::className));
        return true;
    }();
    (void)registered;
}

compat::RetainedString NotificationPopup::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t NotificationPopup::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool NotificationPopup::opEquals(std::shared_ptr<NotificationPopup> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int NotificationPopup::opCmp(std::shared_ptr<NotificationPopup>) {
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

std::shared_ptr<NotificationPopup> NotificationPopup::instance() {
    if (!instance_) {
        instance_ = std::allocate_shared<NotificationPopup>(
            nicxlive::core::d_runtime::SourceAllocator<NotificationPopup>{});
    }
    return instance_;
}

compat::RetainedStringSlice NotificationPopup::status() {
    for (auto index = items_.size(); index > 0; --index) {
        const auto& item = items_[index - 1];
        if (!item.closed && item.message) return item.message->retainedString();
    }
    return compat::RetainedString::emptyLiteral();
}

std::uint64_t NotificationPopup::popup(compat::RetainedStringSlice text, float duration) {
    Item item;
    item.id = nextId_++;
    item.message.emplace(text);
    compat::detail::ngSourceCStringPostconditions(text.view(), item.message->data());
    item.remaining = duration;
    item.infinite = duration < 0;
    items_.append(item);
    return item.id;
}

std::uint64_t NotificationPopup::popup(std::optional<std::string> text, float duration) {
    const auto retained = [&] {
        try { return text ? compat::RetainedString(std::move(*text)) : compat::RetainedString{}; }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    return popup(compat::RetainedStringSlice(retained), duration);
}

std::uint64_t NotificationPopup::popup(Callback callback, float duration) {
    Item item;
    item.id = nextId_++;
    item.remaining = duration;
    item.infinite = duration < 0;
    item.callback = callback;
    items_.append(item);
    return item.id;
}

void NotificationPopup::clearAll() {
    items_ = items_.slice(0, 0);
}

void NotificationPopup::close(std::uint64_t id) {
    for (std::size_t index = 0; index < items_.size(); ++index) {
        if (items_[index].id == id) items_[index].closed = true;
    }
}

bool NotificationPopup::isOpen(std::uint64_t id) const {
    for (std::size_t index = 0; index < items_.size(); ++index) {
        if (items_[index].id == id && !items_[index].closed) return true;
    }
    return false;
}

} // namespace nijigenerate::widgets
