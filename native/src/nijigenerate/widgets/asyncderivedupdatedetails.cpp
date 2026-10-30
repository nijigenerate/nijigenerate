// Copyright 2026, nijigenerate Project. Distributed under the 2-Clause BSD License.
#include "asyncderivedupdatedetails.hpp"
#include "asyncderivedupdatedetails_sort.hpp"
#include "button.hpp"
#include "label.hpp"
#include "tooltip.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <span>

namespace nijigenerate::widgets::asyncderivedupdatedetails {
namespace {
using namespace core::asyncderivedupdate;

template<std::size_t N>
compat::RetainedString translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}

template<class... Args>
compat::RetainedString format(std::string_view pattern, Args&&... values) {
    const std::array<core::translation::TypedFormatArgument, sizeof...(Args)> arguments{
        core::translation::ngFormatArgument(std::forward<Args>(values))...};
    auto formatted = core::translation::ngFormatTyped(pattern, arguments);
    try { return compat::RetainedString(std::move(formatted)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

ImVec4 stateColor(AsyncDerivedUpdateState state) {
    switch (state) {
    case AsyncDerivedUpdateState::Detected: return {1.0f, .55f, .1f, 1.0f};
    case AsyncDerivedUpdateState::Queued: return {.72f, .35f, 1.0f, 1.0f};
    case AsyncDerivedUpdateState::Running: return {.1f, .9f, 1.0f, 1.0f};
    case AsyncDerivedUpdateState::Applied: return {.2f, 1.0f, .35f, 1.0f};
    case AsyncDerivedUpdateState::Stale: return {1.0f, .45f, .1f, 1.0f};
    case AsyncDerivedUpdateState::Failed: return {1.0f, .1f, .15f, 1.0f};
    case AsyncDerivedUpdateState::Canceled: return {.55f, .55f, .55f, 1.0f};
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/asyncderivedupdatedetails.d"), 18);
}

compat::RetainedString stateLabel(AsyncDerivedUpdateState state) {
    switch (state) {
    case AsyncDerivedUpdateState::Detected: return translated("DETECTED");
    case AsyncDerivedUpdateState::Queued: return translated("WAIT");
    case AsyncDerivedUpdateState::Running: return translated("RUN");
    case AsyncDerivedUpdateState::Applied: return translated("OK");
    case AsyncDerivedUpdateState::Stale: return translated("STALE");
    case AsyncDerivedUpdateState::Failed: return translated("ERROR");
    case AsyncDerivedUpdateState::Canceled: return translated("CANCELED");
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/asyncderivedupdatedetails.d"), 30);
}

compat::RetainedString progressText(const AsyncDerivedUpdateSnapshot& snapshot) {
    if (!snapshot.expectedUnits) return {};
    return format(" %s/%s", snapshot.appliedUnits, snapshot.expectedUnits);
}
}

void drawAsyncDerivedUpdateDetailsUi(const compat::Slice<AsyncDerivedUpdateSnapshot>& snapshots) {
    std::size_t detected = 0, queued = 0, running = 0, applied = 0;
    std::size_t stale = 0, failed = 0, canceled = 0, detailCount = 0;
    constexpr auto details = static_cast<std::uint32_t>(AsyncDerivedUpdateDisplay::Details);
    for (const auto& snapshot : snapshots) {
        if (!(snapshot.display & details)) continue;
        ++detailCount;
        switch (snapshot.state) {
        case AsyncDerivedUpdateState::Detected: ++detected; break;
        case AsyncDerivedUpdateState::Queued: ++queued; break;
        case AsyncDerivedUpdateState::Running: ++running; break;
        case AsyncDerivedUpdateState::Applied: ++applied; break;
        case AsyncDerivedUpdateState::Stale: ++stale; break;
        case AsyncDerivedUpdateState::Failed: ++failed; break;
        case AsyncDerivedUpdateState::Canceled: ++canceled; break;
        default: throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/asyncderivedupdatedetails.d"), 62);
        }
    }
    if (!detailCount) return;
    const auto summary = format(translated("Updates  wait:%s  run:%s  ok:%s  stale:%s  error:%s").view(),
        queued + detected, running, applied, stale + canceled, failed);
    const auto summaryState = failed ? AsyncDerivedUpdateState::Failed :
        running ? AsyncDerivedUpdateState::Running :
        queued + detected ? AsyncDerivedUpdateState::Queued :
        stale + canceled ? AsyncDerivedUpdateState::Stale : AsyncDerivedUpdateState::Applied;
    const compat::SourceCString summaryText(summary);
    compat::detail::ngSourceCStringPostconditions(summary.view(), summaryText.data());
    const ImVec2 summarySize(0, 26);
    const auto summaryColor = stateColor(summaryState);
    if (ngButtonColored(summaryText.data(), summarySize, summaryColor))
        ImGui::OpenPopup("AsyncDerivedUpdateDetails");
    ngTooltip(compat::RetainedStringSlice(
        translated("Updates derived asynchronously from another edit. Click for details.")));
    if (ImGui::BeginPopup("AsyncDerivedUpdateDetails")) {
        ngTextLabel(compat::RetainedStringSlice(translated("Derived Updates")));
        compat::Slice<std::size_t> indices;
        for (std::size_t i = 0; i < snapshots.size(); ++i)
            if (snapshots[i].display & details) indices.append(i);
        auto before = [&](std::size_t a, std::size_t b) {
            return snapshots[a].label.view() < snapshots[b].label.view();
        };
        sort_detail::Sorter(before).sort({indices.data(), indices.size()});
        for (auto i : indices) {
            // Retain the current backing while the source reference crosses UI calls.
            const auto snapshotOwner = snapshots;
            if (i >= snapshotOwner.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                i, snapshotOwner.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/asyncderivedupdatedetails.d"), 99);
            const auto& snapshot = snapshotOwner.data()[i];
            const auto operation = snapshot.origin.operationName.empty() ? translated("Update") :
                snapshot.origin.operationName.retainedString();
            // D evaluates these provider calls in source argument order.
            const auto color = stateColor(snapshot.state);
            const auto label = snapshot.label;
            const auto status = stateLabel(snapshot.state);
            const auto progress = progressText(snapshot);
            ngTextColored(color, compat::RetainedStringSlice(format("%s  %s%s", label, status, progress)));
            ngTextLabel(compat::RetainedStringSlice(format(translated("Operation: %s").view(), operation)));
            if (!snapshot.reason.empty())
                ngTextLabel(compat::RetainedStringSlice(format(translated("Reason: %s").view(), snapshot.reason)));
            if (snapshot.retryCount)
                ngTextLabel(compat::RetainedStringSlice(format(translated("Retry: %s").view(), snapshot.retryCount)));
            if (!snapshot.detail.empty())
                ngTextLabel(compat::RetainedStringSlice(format(translated("Detail: %s").view(), snapshot.detail)));
            ImGui::Separator();
        }
        ImGui::EndPopup();
    }
}
} // namespace nijigenerate::widgets::asyncderivedupdatedetails
