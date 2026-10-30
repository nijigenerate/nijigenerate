#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/widgets/notification_item.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <compat/object_metadata.hpp>

// Opaque callback argument; the selected ImGui integration owns its definition.
struct ImGuiIO;

namespace nijigenerate::widgets {

class NotificationPopup : public std::enable_shared_from_this<NotificationPopup> {
private:
    using Item = detail::NotificationItem;
    compat::Slice<Item> items_{};
    std::uint64_t nextId_ = 1;
protected:
    static thread_local std::shared_ptr<NotificationPopup> instance_;
public:
    using Callback = compat::SharedDelegate<void(ImGuiIO*)>;

    NotificationPopup();
    virtual ~NotificationPopup();
    NotificationPopup(const NotificationPopup&) = delete;
    NotificationPopup& operator=(const NotificationPopup&) = delete;

    static std::shared_ptr<NotificationPopup> instance();
    virtual compat::RetainedStringSlice status();
    virtual std::uint64_t popup(compat::RetainedStringSlice text, float duration);
    // Native compatibility input converts into the same retained virtual entry.
    std::uint64_t popup(std::optional<std::string> text, float duration);
    virtual std::uint64_t popup(Callback callback, float duration);
    virtual void onUpdate();
    virtual void clearAll();
    virtual void close(std::uint64_t id);
    virtual bool isOpen(std::uint64_t id) const;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<NotificationPopup> other);
    virtual int opCmp(std::shared_ptr<NotificationPopup> other);

protected:
    virtual void prepareSourceClassInfo();
};

} // namespace nijigenerate::widgets
namespace nijigenerate::widgets::notification {
using ::nijigenerate::widgets::NotificationPopup;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::NotificationPopup> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.notification.NotificationPopup)";
    inline static constexpr char unqualifiedArgument[] = "(NotificationPopup)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.notification.NotificationPopup";
    inline static constexpr char unqualifiedElement[] = "NotificationPopup";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::NotificationPopup> {
    inline static constexpr char className[] = "nijigenerate.widgets.notification.NotificationPopup";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.notification.NotificationPopup)";
};
}
