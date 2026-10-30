#pragma once

#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_vec3.hpp>
#include <cstddef>
#include <tuple>
#include <array>

#include <nijigenerate/compat/phobos_monotime.hpp>
#include <cstdint>
#include <memory>
#include <limits>
#include <cstring>
#include <span>
#include <math/types.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::core::asyncderivedupdate {

enum class AsyncDerivedUpdateState { Detected, Queued, Running, Applied, Stale, Failed, Canceled };
enum class AsyncDerivedUpdateMergePolicy { CoalescePending, ExtendRunning, SupersedeRunning };
enum class AsyncDerivedUpdateDisplay : std::uint32_t {
    None = 0, ViewportBadge = 1, ViewportOutline = 2, Details = 4
};
enum class AsyncDerivedUpdateViewportChannel : std::uint32_t {
    None = 0, Model = 1, Depth = 2, Any = UINT32_MAX
};

struct AsyncDerivedUpdateRunHandle {
    std::uint64_t id{};
    bool valid() const { return id != 0; }
    bool operator==(const AsyncDerivedUpdateRunHandle&) const = default;
};
struct AsyncDerivedUpdateTargetHandle {
    std::uint64_t id{};
    bool valid() const { return id != 0; }
    bool operator==(const AsyncDerivedUpdateTargetHandle&) const = default;
};
struct AsyncDerivedUpdateWorkHandle {
    std::uint64_t id{};
    bool valid() const { return id != 0; }
    bool operator==(const AsyncDerivedUpdateWorkHandle&) const = default;
};
struct AsyncDerivedUpdateScopeId {
    std::uint64_t id{};
    bool valid() const { return id != 0; }
    bool operator==(const AsyncDerivedUpdateScopeId&) const = default;
};
struct AsyncDerivedUpdateOrigin {
    AsyncDerivedUpdateScopeId projectScope;
    std::uint64_t transactionId{};
    std::uint32_t providerId{};
    compat::RetainedStringSlice operationName;
    bool operator==(const AsyncDerivedUpdateOrigin&) const = default;
};
struct AsyncDerivedUpdateTargetKey {
    std::uint32_t providerId{};
    std::uint64_t scopeId{};
    std::uint64_t subjectId{};
    // This integral-only source struct uses TypeInfo_Struct's bytewise hash
    // and equality. Preserve its zero-initialized padding and full-value copies.
    AsyncDerivedUpdateTargetKey() noexcept { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }
    AsyncDerivedUpdateTargetKey(std::uint32_t provider, std::uint64_t scope = 0,
        std::uint64_t subject = 0) noexcept
        : AsyncDerivedUpdateTargetKey() {
        providerId = provider;
        scopeId = scope;
        subjectId = subject;
    }
    AsyncDerivedUpdateTargetKey(const AsyncDerivedUpdateTargetKey& other) noexcept {
        std::memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
    }
    AsyncDerivedUpdateTargetKey& operator=(const AsyncDerivedUpdateTargetKey& other) noexcept {
        if (this != &other)
            std::memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
        return *this;
    }
    bool operator==(const AsyncDerivedUpdateTargetKey& other) const noexcept {
        return std::memcmp(this, &other, sizeof(*this)) == 0;
    }
};

// Retained read-only element view; mutable producer slices may share its backing.
class AsyncDerivedUpdateOutline {
public:
    using Vec3 = nicxlive::core::math::Vec3;
    AsyncDerivedUpdateOutline() = default;
    explicit AsyncDerivedUpdateOutline(compat::Slice<Vec3> storage) : storage_(std::move(storage)) {}
    bool isNull() const { return storage_.isNull(); }
    std::size_t size() const { return storage_.size(); }
    const Vec3* data() const { return storage_.data(); }
    const Vec3& operator[](std::size_t index) const { return storage_[index]; }
    std::span<const Vec3> view() const { return {storage_.data(), storage_.size()}; }
    AsyncDerivedUpdateOutline slice(std::size_t first, std::size_t last) const {
        return AsyncDerivedUpdateOutline(storage_.slice(first, last));
    }
    AsyncDerivedUpdateOutline duplicate() const { return AsyncDerivedUpdateOutline(storage_.duplicate()); }
    void reset() { storage_.clear(); }
    void resize(std::size_t count) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        storage_.resize(count, Vec3{nan, nan, nan});
    }
private:
    compat::Slice<Vec3> storage_;
};
struct AsyncDerivedUpdateViewportVisual {
    std::uint32_t viewportChannel{};
    bool hasAnchor{};
    nicxlive::core::math::Vec3 anchorWorld{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    AsyncDerivedUpdateOutline outlineWorld;
};
struct AsyncDerivedUpdateTargetDesc {
    AsyncDerivedUpdateTargetKey key;
    compat::RetainedString label;
    compat::RetainedString reason;
    std::uint64_t sourceRevision{};
    AsyncDerivedUpdateMergePolicy mergePolicy = AsyncDerivedUpdateMergePolicy::SupersedeRunning;
    std::uint32_t display = 7;
    AsyncDerivedUpdateViewportVisual visual;
};
struct AsyncDerivedUpdateSnapshot {
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetHandle target;
    AsyncDerivedUpdateOrigin origin;
    AsyncDerivedUpdateTargetKey key;
    compat::RetainedString label;
    compat::RetainedString reason;
    compat::RetainedString detail;
    AsyncDerivedUpdateState state{};
    std::uint64_t sourceRevision{};
    std::uint64_t expectedUnits{};
    std::uint64_t appliedUnits{};
    std::uint64_t queuedUnits{};
    std::uint64_t runningUnits{};
    std::uint64_t staleUnits{};
    std::uint32_t retryCount{};
    std::uint32_t display{};
    AsyncDerivedUpdateViewportVisual visual;
    compat::SourceMonoTime changedAt{};
};

AsyncDerivedUpdateScopeId incAsyncDerivedUpdateCreateScope();
AsyncDerivedUpdateRunHandle incAsyncDerivedUpdateBeginRun(const AsyncDerivedUpdateOrigin& origin);
void incAsyncDerivedUpdateSetOwnerToken(AsyncDerivedUpdateRunHandle handle, actions::AsyncActionToken token);
AsyncDerivedUpdateTargetHandle incAsyncDerivedUpdateTrackTarget(
    AsyncDerivedUpdateRunHandle run, AsyncDerivedUpdateTargetDesc& desc);
void incAsyncDerivedUpdateSetExpected(AsyncDerivedUpdateTargetHandle handle, std::uint64_t units);
void incAsyncDerivedUpdateSetContext(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString reason,
    compat::RetainedString detail = {}, std::uint32_t retryCount = 0);
AsyncDerivedUpdateWorkHandle incAsyncDerivedUpdateQueue(AsyncDerivedUpdateTargetHandle handle,
    std::uint64_t units = 1);
void incAsyncDerivedUpdateStart(AsyncDerivedUpdateWorkHandle handle);
void incAsyncDerivedUpdateApplied(AsyncDerivedUpdateWorkHandle handle);
void incAsyncDerivedUpdateRetry(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString reason);
void incAsyncDerivedUpdateStale(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail);
void incAsyncDerivedUpdateDiscard(AsyncDerivedUpdateWorkHandle handle);
void incAsyncDerivedUpdateMarkTargetStale(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail,
    std::uint64_t units = 1);
void incAsyncDerivedUpdateFailed(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail);
void incAsyncDerivedUpdateCanceled(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail);
void incAsyncDerivedUpdateFailTarget(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail);
void incAsyncDerivedUpdateCancelTarget(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail);
void incAsyncDerivedUpdateSeal(AsyncDerivedUpdateTargetHandle handle);
void incAsyncDerivedUpdateEndRun(AsyncDerivedUpdateRunHandle handle);
bool incAsyncDerivedUpdateTargetSnapshot(AsyncDerivedUpdateTargetHandle handle,
    AsyncDerivedUpdateSnapshot& snapshot, bool includeExpired = false);
compat::Slice<AsyncDerivedUpdateSnapshot> incAsyncDerivedUpdateSnapshots(AsyncDerivedUpdateScopeId projectScope,
    std::uint32_t viewportChannel = 0, bool includeExpired = false);
float incAsyncDerivedUpdateProgress(const AsyncDerivedUpdateSnapshot& snapshot);
bool incAsyncDerivedUpdateShowsViewportProgress(const AsyncDerivedUpdateSnapshot& snapshot);
void incAsyncDerivedUpdateForgetTarget(AsyncDerivedUpdateTargetHandle handle);
void incAsyncDerivedUpdateUpdate();
void incAsyncDerivedUpdateClearScope(AsyncDerivedUpdateScopeId projectScope);
void incAsyncDerivedUpdateClearProvider(std::uint32_t providerId);
void incAsyncDerivedUpdateClearAll();

} // namespace nijigenerate::core::asyncderivedupdate

namespace nijigenerate::core::translation {

template<> struct FormatSourceTypeName<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateOutline> {
    static std::string get() { return detail::copyFormatTypeName("const(Vector!(float, 3))[]"); }
};

} // namespace nijigenerate::core::translation

namespace nijigenerate::core::asyncderivedupdate {

inline translation::TypedFormatArgument ngFormatArgument(AsyncDerivedUpdateOutline values) {
    using namespace translation;
    auto range = std::allocate_shared<FormatRange>(nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
    range->length = values.size();
    range->sourceTypeName = FormatSourceTypeName<
        ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateOutline>::get();
    auto read = [values = std::move(values)](std::size_t index) {
        return translation::ngFormatArgument(values[index]);
    };
    translation::detail::allocateAggregateText([&] { range->read = std::move(read); });
    return range;
}

} // namespace nijigenerate::core::asyncderivedupdate

namespace nijigenerate::core::translation {

#define NG_ASYNC_HANDLE_FORMAT(Type) \
template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::Type> { \
    using Value = ::nijigenerate::core::asyncderivedupdate::Type; \
    inline static constexpr std::string_view typeName = #Type; \
    static constexpr auto fields() { \
        return std::tuple{ngFormatAggregateMember("id", offsetof(Value, id), &Value::id)}; \
    } \
};
NG_ASYNC_HANDLE_FORMAT(AsyncDerivedUpdateRunHandle)
NG_ASYNC_HANDLE_FORMAT(AsyncDerivedUpdateTargetHandle)
NG_ASYNC_HANDLE_FORMAT(AsyncDerivedUpdateWorkHandle)
NG_ASYNC_HANDLE_FORMAT(AsyncDerivedUpdateScopeId)
#undef NG_ASYNC_HANDLE_FORMAT

template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateOrigin> {
    using Value = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateOrigin;
    inline static constexpr std::string_view typeName = "AsyncDerivedUpdateOrigin";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("projectScope", offsetof(Value, projectScope), &Value::projectScope),
            ngFormatAggregateMember("transactionId", offsetof(Value, transactionId), &Value::transactionId),
            ngFormatAggregateMember("providerId", offsetof(Value, providerId), &Value::providerId),
            ngFormatAggregateMember("operationName", offsetof(Value, operationName), &Value::operationName)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetKey> {
    using Value = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetKey;
    inline static constexpr std::string_view typeName = "AsyncDerivedUpdateTargetKey";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("providerId", offsetof(Value, providerId), &Value::providerId),
            ngFormatAggregateMember("scopeId", offsetof(Value, scopeId), &Value::scopeId),
            ngFormatAggregateMember("subjectId", offsetof(Value, subjectId), &Value::subjectId)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportVisual> {
    using Value = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportVisual;
    inline static constexpr std::string_view typeName = "AsyncDerivedUpdateViewportVisual";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("viewportChannel", offsetof(Value, viewportChannel), &Value::viewportChannel),
            ngFormatAggregateMember("hasAnchor", offsetof(Value, hasAnchor), &Value::hasAnchor),
            ngFormatAggregateMember("anchorWorld", offsetof(Value, anchorWorld), &Value::anchorWorld),
            ngFormatAggregateMember("outlineWorld", offsetof(Value, outlineWorld), &Value::outlineWorld)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetDesc> {
    using Value = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetDesc;
    inline static constexpr std::string_view typeName = "AsyncDerivedUpdateTargetDesc";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("key", offsetof(Value, key), &Value::key),
            ngFormatAggregateMember("label", offsetof(Value, label), &Value::label),
            ngFormatAggregateMember("reason", offsetof(Value, reason), &Value::reason),
            ngFormatAggregateMember("sourceRevision", offsetof(Value, sourceRevision), &Value::sourceRevision),
            ngFormatAggregateMember("mergePolicy", offsetof(Value, mergePolicy), &Value::mergePolicy),
            ngFormatAggregateMember("display", offsetof(Value, display), &Value::display),
            ngFormatAggregateMember("visual", offsetof(Value, visual), &Value::visual)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateSnapshot> {
    using Value = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateSnapshot;
    inline static constexpr std::string_view typeName = "AsyncDerivedUpdateSnapshot";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("run", offsetof(Value, run), &Value::run),
            ngFormatAggregateMember("target", offsetof(Value, target), &Value::target),
            ngFormatAggregateMember("origin", offsetof(Value, origin), &Value::origin),
            ngFormatAggregateMember("key", offsetof(Value, key), &Value::key),
            ngFormatAggregateMember("label", offsetof(Value, label), &Value::label),
            ngFormatAggregateMember("reason", offsetof(Value, reason), &Value::reason),
            ngFormatAggregateMember("detail", offsetof(Value, detail), &Value::detail),
            ngFormatAggregateMember("state", offsetof(Value, state), &Value::state),
            ngFormatAggregateMember("sourceRevision", offsetof(Value, sourceRevision), &Value::sourceRevision),
            ngFormatAggregateMember("expectedUnits", offsetof(Value, expectedUnits), &Value::expectedUnits),
            ngFormatAggregateMember("appliedUnits", offsetof(Value, appliedUnits), &Value::appliedUnits),
            ngFormatAggregateMember("queuedUnits", offsetof(Value, queuedUnits), &Value::queuedUnits),
            ngFormatAggregateMember("runningUnits", offsetof(Value, runningUnits), &Value::runningUnits),
            ngFormatAggregateMember("staleUnits", offsetof(Value, staleUnits), &Value::staleUnits),
            ngFormatAggregateMember("retryCount", offsetof(Value, retryCount), &Value::retryCount),
            ngFormatAggregateMember("display", offsetof(Value, display), &Value::display),
            ngFormatAggregateMember("visual", offsetof(Value, visual), &Value::visual),
            ngFormatAggregateMember("changedAt", offsetof(Value, changedAt), &Value::changedAt)};
    }
};

template<> struct FormatEnumTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateState> {
    using Enum = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateState;
    inline static constexpr std::string_view name = "AsyncDerivedUpdateState";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Detected, "Detected"},
        FormatEnumMember<Enum>{Enum::Queued, "Queued"},
        FormatEnumMember<Enum>{Enum::Running, "Running"},
        FormatEnumMember<Enum>{Enum::Applied, "Applied"},
        FormatEnumMember<Enum>{Enum::Stale, "Stale"},
        FormatEnumMember<Enum>{Enum::Failed, "Failed"},
        FormatEnumMember<Enum>{Enum::Canceled, "Canceled"}};
};
template<> struct FormatEnumTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateMergePolicy> {
    using Enum = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateMergePolicy;
    inline static constexpr std::string_view name = "AsyncDerivedUpdateMergePolicy";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::CoalescePending, "CoalescePending"},
        FormatEnumMember<Enum>{Enum::ExtendRunning, "ExtendRunning"},
        FormatEnumMember<Enum>{Enum::SupersedeRunning, "SupersedeRunning"}};
};
template<> struct FormatEnumTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateDisplay> {
    using Enum = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateDisplay;
    inline static constexpr std::string_view name = "AsyncDerivedUpdateDisplay";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::None, "None"},
        FormatEnumMember<Enum>{Enum::ViewportBadge, "ViewportBadge"},
        FormatEnumMember<Enum>{Enum::ViewportOutline, "ViewportOutline"},
        FormatEnumMember<Enum>{Enum::Details, "Details"}};
};
template<> struct FormatEnumTraits<::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel> {
    using Enum = ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel;
    inline static constexpr std::string_view name = "AsyncDerivedUpdateViewportChannel";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::None, "None"},
        FormatEnumMember<Enum>{Enum::Model, "Model"},
        FormatEnumMember<Enum>{Enum::Depth, "Depth"},
        FormatEnumMember<Enum>{Enum::Any, "Any"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateState)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateState";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateState";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateMergePolicy> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateMergePolicy)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateMergePolicy)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateMergePolicy";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateMergePolicy";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateDisplay> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateDisplay)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateDisplay)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateDisplay";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateDisplay";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateViewportChannel)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateViewportChannel)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateViewportChannel";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateViewportChannel";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateRunHandle> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateRunHandle)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateRunHandle)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateRunHandle";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateRunHandle";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetHandle> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetHandle)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateTargetHandle)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetHandle";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateTargetHandle";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateWorkHandle> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateWorkHandle)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateWorkHandle)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateWorkHandle";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateWorkHandle";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateScopeId> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateScopeId)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateScopeId)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateScopeId";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateScopeId";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateOrigin> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateOrigin)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateOrigin)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateOrigin";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateOrigin";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetKey> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetKey)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateTargetKey)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetKey";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateTargetKey";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateViewportVisual> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateViewportVisual)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateViewportVisual)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateViewportVisual";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateViewportVisual";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateTargetDesc> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetDesc)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateTargetDesc)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateTargetDesc";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateTargetDesc";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::AsyncDerivedUpdateSnapshot> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateSnapshot)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncDerivedUpdateSnapshot)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.AsyncDerivedUpdateSnapshot";
    inline static constexpr char unqualifiedElement[] = "AsyncDerivedUpdateSnapshot";
};
}
