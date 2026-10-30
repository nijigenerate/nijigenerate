#include "session.hpp"
#include "nijigenerate/commands/depth/map.hpp"
#include <nodes/deformable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <common/object_key_policy.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::viewport::depth::common::session {
using View = std::shared_ptr<targetview::DepthTargetView>;
using Target = std::shared_ptr<nicxlive::core::nodes::Deformable>;

namespace {
View sourceTargetAt(compat::Slice<View> values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/depth/common/session.d"), line);
    return values.data()[index];
}
compat::Slice<View> sourceTargetSlice(compat::Slice<View> values, std::size_t first,
        std::size_t last) {
    if (first > last || last > values.size())
        throw compat::ngStaticError<compat::ArraySliceError>(first, last, values.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/depth/common/session.d"), 92);
    return values.slice(first, last);
}
}

std::shared_ptr<actions::depth::DepthMappedChangeAction> ngDepthViewWorkingDepthsChangeAction(
    View view, compat::RetainedStringSlice reason) {
    if (!view || !view->getTarget()) return nullptr;
    const auto target = view->getTarget();
    const auto depths = view->copyWorkingDepths();
    return commands::depth::map::ngApplyDepthsChangeAction(target, depths, reason);
}

std::ptrdiff_t DepthViewSession::findTargetIndex(std::uint64_t gridUuid) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto iterationTargets = targets;
    for (std::size_t i = 0; i < iterationTargets.size(); ++i) {
        const auto view = iterationTargets.get(i);
        if (view && view->getTarget() && compat::ngRequireObject(view->getTarget())->getUUID() == gridUuid)
            return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

View DepthViewSession::ensureTarget(Target target) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!target) return nullptr;
    if (auto existing = targetByGrid(target->getUUID())) return existing;
    auto view = std::allocate_shared<targetview::DepthTargetView>(
        nicxlive::core::d_runtime::SourceAllocator<targetview::DepthTargetView>{}, target);
    view->setDepthDisplayScale(depthDisplayScale);
    targets.append(view);
    if (selectedGridUuid == 0) selectedGridUuid = target->getUUID();
    return view;
}

void DepthViewSession::setTargets(compat::Slice<Target> nextTargets) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::Slice<View> nextViews;
    const auto containsNext = [&nextViews](std::uint64_t uuid) {
        const auto iterationViews = nextViews;
        for (std::size_t i = 0; i < iterationViews.size(); ++i) {
            const auto view = iterationViews.get(i);
            if (view && view->getTarget() && compat::ngRequireObject(view->getTarget())->getUUID() == uuid) return true;
        }
        return false;
    };
    for (std::size_t i = 0; i < nextTargets.size(); ++i) {
        const auto target = nextTargets.get(i);
        if (!target || containsNext(target->getUUID())) continue;
        auto view = targetByGrid(target->getUUID());
        if (!view) view = std::allocate_shared<targetview::DepthTargetView>(
            nicxlive::core::d_runtime::SourceAllocator<targetview::DepthTargetView>{}, target);
        view->setDepthDisplayScale(depthDisplayScale);
        nextViews.append(view);
    }
    targets = nextViews;
    if (selectedGridUuid != 0 && targetByGrid(selectedGridUuid)) return;
    selectedGridUuid = targets.size() > 0 && sourceTargetAt(targets, 0, 61) &&
        compat::ngRequireObject(sourceTargetAt(targets, 0, 61))->getTarget()
        ? compat::ngRequireObject(compat::ngRequireObject(sourceTargetAt(targets, 0, 62))->getTarget())->getUUID() : 0;
}

void DepthViewSession::setDepthDisplayScale(float value) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    depthDisplayScale = value > 0.0f ? value : 0.0f;
    const auto iterationTargets = targets;
    for (std::size_t i = 0; i < iterationTargets.size(); ++i) {
        const auto view = iterationTargets.get(i);
        if (view) view->setDepthDisplayScale(depthDisplayScale);
    }
}

View DepthViewSession::targetByGrid(std::uint64_t gridUuid) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto index = findTargetIndex(gridUuid);
    return index >= 0 ? sourceTargetAt(targets, static_cast<std::size_t>(index), 75) : nullptr;
}

bool DepthViewSession::selectTarget(std::uint64_t gridUuid) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!targetByGrid(gridUuid)) return false;
    selectedGridUuid = gridUuid;
    return true;
}

View DepthViewSession::selectedTarget() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return targetByGrid(selectedGridUuid);
}

void DepthViewSession::removeTarget(std::uint64_t gridUuid) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto index = findTargetIndex(gridUuid);
    if (index < 0) return;
    const auto i = static_cast<std::size_t>(index);
    const auto prefix = sourceTargetSlice(targets, 0, i);
    const auto suffix = sourceTargetSlice(targets, i + 1, targets.size());
    compat::Slice<View> nextViews;
    // The authored concatenation evaluates both slices before allocating its
    // independent result. Keep one result allocation rather than two appends.
    nextViews.resize(prefix.size() + suffix.size());
    for (std::size_t j = 0; j < prefix.size(); ++j) nextViews.data()[j] = prefix.data()[j];
    for (std::size_t j = 0; j < suffix.size(); ++j) nextViews.data()[prefix.size() + j] = suffix.data()[j];
    targets = nextViews;
    if (selectedGridUuid == gridUuid) {
        selectedGridUuid = targets.size() > 0 && sourceTargetAt(targets, 0, 94) &&
            compat::ngRequireObject(sourceTargetAt(targets, 0, 94))->getTarget()
            ? compat::ngRequireObject(compat::ngRequireObject(sourceTargetAt(targets, 0, 95))->getTarget())->getUUID() : 0;
    }
}

void DepthViewSession::refreshGeometry() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto iterationTargets = targets;
    for (std::size_t i = 0; i < iterationTargets.size(); ++i) {
        const auto view = iterationTargets.get(i);
        if (view) view->refreshGeometry();
    }
}

void DepthViewSession::resetWorkingDepths() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto iterationTargets = targets;
    for (std::size_t i = 0; i < iterationTargets.size(); ++i) {
        const auto view = iterationTargets.get(i);
        if (view) view->resetWorkingDepths();
    }
}

void DepthViewSession::resetFromTargets() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto iterationTargets = targets;
    for (std::size_t i = 0; i < iterationTargets.size(); ++i) {
        const auto view = iterationTargets.get(i);
        if (view) view->resetFromTarget();
    }
}

std::shared_ptr<actions::depth::DepthMappedChangeAction> DepthViewSession::workingDepthsChangeAction(
    std::uint64_t gridUuid, compat::RetainedStringSlice reason) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return ngDepthViewWorkingDepthsChangeAction(targetByGrid(gridUuid), reason);
}

std::shared_ptr<actions::depth::DepthMappedChangeAction> DepthViewSession::selectedWorkingDepthsChangeAction(
    compat::RetainedStringSlice reason) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return workingDepthsChangeAction(selectedGridUuid, reason);
}

void DepthViewSession::clear() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    targets = {};
    selectedGridUuid = 0;
    depthDisplayScale = 0.0f;
}

// object.Object defaults (Digital Mars 2000-2011), BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DepthViewSession::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthViewSession),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.depth.common.session.DepthViewSession"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString DepthViewSession::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthViewSession::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthViewSession::opEquals(std::shared_ptr<DepthViewSession> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthViewSession::opCmp(std::shared_ptr<DepthViewSession>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDepthViewSessionObjectEquals(std::shared_ptr<DepthViewSession> left,
        std::shared_ptr<DepthViewSession> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

} // namespace nijigenerate::viewport::depth::common::session
