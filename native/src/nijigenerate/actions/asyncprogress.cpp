#include "asyncprogress.hpp"
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/static_error.hpp>
#include <common/object_key_policy.hpp>
#include <imgui.h>
#include <algorithm>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::actions::asyncprogress {
namespace {
template<class Allocate> decltype(auto) allocateProgressBacking(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void prepareAsyncProgressClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AsyncGroupActionProgress),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.asyncprogress.AsyncGroupActionProgress"));
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString AsyncGroupActionProgress::toString() {
    prepareAsyncProgressClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AsyncGroupActionProgress::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool AsyncGroupActionProgress::opEquals(std::shared_ptr<AsyncGroupActionProgress> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int AsyncGroupActionProgress::opCmp(std::shared_ptr<AsyncGroupActionProgress>) {
    prepareAsyncProgressClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = allocateProgressBacking([&] {
        std::string bytes = "need opCmp for class ";
        bytes += className.view();
        return compat::RetainedString(std::move(bytes));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngAsyncProgressObjectEquals(std::shared_ptr<AsyncGroupActionProgress> left,
    std::shared_ptr<AsyncGroupActionProgress> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareAsyncProgressClassInfo);
}
std::shared_ptr<AsyncGroupActionProgress> AsyncGroupActionProgress::create(
    std::shared_ptr<AsyncGroupAction> action, compat::RetainedString title,
    compat::RetainedString completedMessage, compat::RetainedString canceledMessage,
    WorkCountHandler workCount, PumpHandler pump) {
    return ngCreateDerived<AsyncGroupActionProgress>(std::move(action), std::move(title),
        std::move(completedMessage), std::move(canceledMessage), std::move(workCount), std::move(pump));
}
void AsyncGroupActionProgress::initialize(std::shared_ptr<AsyncGroupAction> action,
    compat::RetainedString title, compat::RetainedString completedMessage,
    compat::RetainedString canceledMessage, WorkCountHandler workCount, PumpHandler pump) {
    action_ = action;
    title_ = title; completedMessage_ = completedMessage;
    canceledMessage_ = canceledMessage; workCount_ = workCount; pump_ = pump;
    const auto self = shared_from_this();
    if (action) {
        self->observerId_ = action->addObserver(allocateProgressBacking([&] {
            return AsyncGroupAction::Observer([self](auto source, auto event) {
                self->onActionEvent(source, event);
            });
        }));
        if (action->isApplied() && action->pendingAsyncCount() > 0) self->activate();
    }
}
bool AsyncGroupActionProgress::isActive() const {
    const auto receiver = weak_from_this().lock();
    const auto action = action_;
    return !disposed_ && action && action->isApplied() && remainingWork() > 0;
}
bool AsyncGroupActionProgress::isVisible() const {
    const auto receiver = weak_from_this().lock();
    return !disposed_ && popupId_ != 0 && widgets::NotificationPopup::instance()->isOpen(popupId_);
}
void AsyncGroupActionProgress::dispose() {
    if (disposed_) return;
    const auto lifetime = shared_from_this();
    disposed_ = true;
    if (auto action = action_) action->removeObserver(observerId_);
    observerId_ = 0; closePopup(); action_.reset(); workCount_.reset(); pump_.reset();
}
void AsyncGroupActionProgress::onActionEvent(std::shared_ptr<AsyncGroupAction> source, AsyncGroupActionEvent event) {
    if (disposed_ || source != action_) return;
    switch (event) {
    case AsyncGroupActionEvent::Scheduled:
    case AsyncGroupActionEvent::Running:
    case AsyncGroupActionEvent::Progressed:
        if (remainingWork() > 0) activate();
        break;
    case AsyncGroupActionEvent::Completed:
        if (remainingWork() > 0) activate(); else complete();
        break;
    case AsyncGroupActionEvent::Failed: closePopup(); break;
    case AsyncGroupActionEvent::Canceled: cancel(); break;
    case AsyncGroupActionEvent::Redone: closePopup(); break;
    default: throw compat::ngStaticError<compat::SwitchError>(
        "No appropriate switch clause found", "source/nijigenerate/actions/asyncprogress.d", 82);
    }
}
void AsyncGroupActionProgress::activate() {
    if (!isActive()) return;
    const auto action = action_;
    const auto generation = compat::ngRequireObject(action)->generation();
    if (popupId_ == 0 || popupGeneration_ != generation) {
        closePopup(); popupGeneration_ = generation;
        observedTotal_ = std::max(std::size_t{1}, remainingWork());
        auto self = shared_from_this();
        popupId_ = widgets::NotificationPopup::instance()->popup([self](ImGuiIO*) { self->draw(); }, -1);
    }
    schedulePump();
}
void AsyncGroupActionProgress::draw() {
    const auto action = action_;
    if (disposed_ || !action || !action->isApplied()) {
        closePopup(); return;
    }
    const auto popupGeneration = popupGeneration_;
    const auto currentGeneration = compat::ngRequireObject(action_)->generation();
    if (popupGeneration != currentGeneration || remainingWork() == 0) {
        closePopup(); return;
    }
    const auto remaining = remainingWork();
    const auto previousTotal = observedTotal_;
    const auto currentCompleted = compat::ngRequireObject(action_)->completedAsyncCount();
    observedTotal_ = std::max(previousTotal, currentCompleted + remaining);
    const auto total = observedTotal_;
    auto completed = total > remaining ? total - remaining : 0;
    completed = std::min(completed, total);
    const float ratio = total > 0 ? static_cast<float>(completed) / static_cast<float>(total) : 0.0f;
    const compat::SourceCString terminated{compat::RetainedStringSlice(title_)};
    ImGui::Text(terminated.data());
    ImGui::ProgressBar(ratio, ImVec2(320, 0));
}
void AsyncGroupActionProgress::schedulePump() {
    if (!pump_ || pumpQueued_ || !isActive()) return;
    pumpQueued_ = true;
    auto self = shared_from_this();
    const auto generation = compat::ngRequireObject(action_)->generation();
    api::mcp::task::ngMcpEnqueueAction(PumpHandler([self, generation] { self->pumpStep(generation); }));
}
void AsyncGroupActionProgress::pumpStep(std::uint64_t generation) {
    pumpQueued_ = false;
    const auto action = action_;
    if (disposed_ || !action) return;
    if (!action->isApplied() || compat::ngRequireObject(action_)->generation() != generation) {
        schedulePump(); return;
    }
    if (remainingWork() == 0) { complete(); return; }
    try { pump_(); }
    catch (const compat::Throwable& throwable) {
        closePopup();
        if (const auto* error = dynamic_cast<const compat::ThrowableError*>(&throwable))
            ::nicxlive::core::d_runtime::ngRethrowSourceError(*error);
        throw;
    }
    if (remainingWork() == 0) complete(); else schedulePump();
}
void AsyncGroupActionProgress::complete() {
    const auto action = action_;
    const auto generation = action ? action->generation() : 0;
    closePopup();
    if (completedMessage_.empty() || generation == notifiedCompletionGeneration_) return;
    notifiedCompletionGeneration_ = generation;
    widgets::NotificationPopup::instance()->popup(compat::RetainedStringSlice(completedMessage_), 3);
}
void AsyncGroupActionProgress::cancel() {
    closePopup();
    if (!canceledMessage_.empty())
        widgets::NotificationPopup::instance()->popup(compat::RetainedStringSlice(canceledMessage_), 3);
}
void AsyncGroupActionProgress::closePopup() {
    if (popupId_ == 0) return;
    widgets::NotificationPopup::instance()->close(popupId_); popupId_ = 0; popupGeneration_ = 0;
}
std::size_t AsyncGroupActionProgress::remainingWork() const {
    const auto action = action_;
    if (!action) return 0;
    return !workCount_ ? action->pendingAsyncCount() : workCount_();
}
}
