#pragma once

#include <nijigenerate/commands/automesh/dynamic_types_fwd.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <compat/source_mutex.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::automesh::dynamic::detail {

// Copies retain the same callable environment, as a D delegate descriptor does.
using ParentCanceled = compat::SharedDelegate<bool()>;

// Preserve existing native callers' same retained callable, without copying its
// mutable environment. Canonical storage remains the shared D delegate provider.
inline ParentCanceled retainParentCanceled(std::shared_ptr<const std::function<bool()>> callback) {
    if (!callback || !*callback) return {};
    return [callback] { return (*callback)(); };
}

class AutoMeshProgressState {
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> lock;

public:
    std::size_t total{};

private:
    std::size_t done_{};
    ::nijigenerate::compat::RetainedStringSlice currentName_;
    bool canceled_{};
    ParentCanceled parentCanceled;

protected:
    virtual void prepareSourceClassInfo();
public:
    explicit AutoMeshProgressState(std::size_t total, ParentCanceled parentCanceled = {});
    virtual ~AutoMeshProgressState() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoMeshProgressState> other);
    virtual int opCmp(std::shared_ptr<AutoMeshProgressState> other);

    // D class values copy a reference, never this object's fields.
    AutoMeshProgressState(const AutoMeshProgressState&) = delete;
    AutoMeshProgressState& operator=(const AutoMeshProgressState&) = delete;
    AutoMeshProgressState(AutoMeshProgressState&&) = delete;
    AutoMeshProgressState& operator=(AutoMeshProgressState&&) = delete;

    virtual void beginTarget(::nijigenerate::compat::RetainedStringSlice name);
    virtual void completeTarget();
    virtual void requestCancel();
    virtual bool canceled();
    virtual void snapshot(std::size_t& done, std::size_t& totalOut,
                          ::nijigenerate::compat::RetainedStringSlice& currentName, bool& canceledOut);
};

}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::automesh::dynamic::detail::AutoMeshProgressState> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.automesh.dynamic.AutoMeshProgressState)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshProgressState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.automesh.dynamic.AutoMeshProgressState";
    inline static constexpr char unqualifiedElement[] = "AutoMeshProgressState";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::automesh::dynamic::detail::AutoMeshProgressState> {
    inline static constexpr char className[] =
        "nijigenerate.commands.automesh.dynamic.AutoMeshProgressState";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.automesh.dynamic.AutoMeshProgressState)";
};
} // namespace nicxlive::core::d_runtime
