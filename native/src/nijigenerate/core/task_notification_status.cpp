#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::core::tasks {

void ngSetStatus(TaskString status) {
    const auto retained = detail::retainNativeTaskString(std::move(status));
    ngSetStatus(compat::RetainedStringSlice(retained));
}

void ngSetStatus(compat::RetainedStringSlice status) {
    (void)compat::ngRequireObject(widgets::NotificationPopup::instance())->popup(
        status, static_cast<float>(statusTimeSet));
}

compat::RetainedStringSlice ngGetStatus() {
    return compat::ngRequireObject(widgets::NotificationPopup::instance())->status();
}

void ngStatusUpdate() {
    compat::ngRequireObject(widgets::NotificationPopup::instance())->onUpdate();
}

} // namespace nijigenerate::core::tasks
