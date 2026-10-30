#pragma once

#include "source_callable.hpp"
#include "native_access_fault.hpp"
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

// The defining publisher supplies a real thunk and an already retained context.
// This is a Source value descriptor, not a binary D delegate representation.
// Primary rules: dlang.org/spec/type.html#delegates and
// dlang.org/spec/function.html#closures; callee acquisition follows
// dlang.org/spec/expression.html#order-calls.
template<class Signature, class Context = void> class SourceDelegate;

template<class R, class Context, class... Args>
class SourceDelegate<R(Args...), Context> {
public:
    using signature_type = R(Args...);
    using context_type = Context;
    using function_type = R(*)(Context*, Args...);
    using context_handle = std::shared_ptr<Context>;

private:
    context_handle context_;
    function_type function_ = nullptr;

    SourceDelegate(context_handle context, function_type function) noexcept
        : context_(std::move(context)), function_(function) {}

public:
    SourceDelegate() noexcept = default;
    SourceDelegate(std::nullptr_t) noexcept {}
    SourceDelegate(const SourceDelegate&) noexcept = default;

    // Moving a Source delegate still acquires the same shallow descriptor.
    SourceDelegate(SourceDelegate&& other) noexcept
        : context_(other.context_), function_(other.function_) {}

    SourceDelegate& operator=(SourceDelegate other) noexcept {
        swap(other);
        return *this;
    }
    SourceDelegate& operator=(std::nullptr_t) noexcept {
        SourceDelegate empty;
        swap(empty);
        return *this;
    }
    void swap(SourceDelegate& other) noexcept {
        context_.swap(other.context_);
        std::swap(function_, other.function_);
    }

    // A null thunk is never published with a non-null context. This leaves only
    // the actual null descriptor and actual bound delegate values constructible.
    template<function_type Function> requires (Function != nullptr)
    static SourceDelegate bind(context_handle context) noexcept {
        return SourceDelegate(std::move(context), Function);
    }

    Context* context() const noexcept { return context_.get(); }
    function_type function() const noexcept { return function_; }
    context_handle retainedContext() const noexcept { return context_; }
    // Published values have either both null fields or a real function, which
    // may have a null context. Retention bookkeeping is not value identity.
    explicit operator bool() const noexcept { return function_ != nullptr; }

    friend bool operator==(const SourceDelegate& left, const SourceDelegate& right) noexcept {
        return left.function_ == right.function_ && left.context_.get() == right.context_.get();
    }
    friend bool operator==(const SourceDelegate& value, std::nullptr_t) noexcept {
        return value.function_ == nullptr;
    }

    template<class... Actual> requires std::is_invocable_r_v<R, function_type, Context*, Actual&&...>
    R opCall(Actual&&... args) const {
        // Keep this exact context alive even if the body rebinds its publisher.
        const auto context = context_;
        const auto function = function_;
        if (!function) ngFailNullAccess(false);
        return function(context.get(), std::forward<Actual>(args)...);
    }
    template<class... Actual> requires std::is_invocable_r_v<R, function_type, Context*, Actual&&...>
    R operator()(Actual&&... args) const {
        return opCall(std::forward<Actual>(args)...);
    }
};

namespace detail {
template<class T> struct SourceDelegateThunk;
template<class R, class Context, class... Args>
struct SourceDelegateThunk<R(*)(Context*, Args...)> {
    using delegate_type = SourceDelegate<R(Args...), Context>;
    using context_type = Context;
};
template<class T> struct IsSourceDelegate : std::false_type {};
template<class Signature, class Context>
struct IsSourceDelegate<SourceDelegate<Signature, Context>> : std::true_type {};
}

// The shared owner can already be an aliasing handle to the published context.
// Neither this factory nor invocation allocates or copies the context object.
template<auto Function, class Owner>
    requires requires {
        typename detail::SourceDelegateThunk<decltype(Function)>::delegate_type;
    } && (Function != nullptr)
    && std::convertible_to<Owner*, typename detail::SourceDelegateThunk<decltype(Function)>::context_type*>
auto ngSourceDelegate(std::shared_ptr<Owner> owner) noexcept {
    using Traits = detail::SourceDelegateThunk<decltype(Function)>;
    using Delegate = typename Traits::delegate_type;
    using Context = typename Traits::context_type;
    return Delegate::template bind<Function>(std::shared_ptr<Context>(std::move(owner)));
}

template<class Signature, class Context>
struct SourceCallPreparation<SourceDelegate<Signature, Context>> {
    static SourceDelegate<Signature, Context> prepare(const SourceDelegate<Signature, Context>& value) noexcept {
        return value;
    }
};

struct SourceDelegateAliasAcquisition {
    template<class Delegate> requires detail::IsSourceDelegate<std::remove_cvref_t<Delegate>>::value
    static std::remove_cvref_t<Delegate> acquire(const Delegate& value) noexcept { return value; }
};

template<auto& Declaration>
    requires detail::IsSourceDelegate<std::remove_cvref_t<decltype(Declaration)>>::value
using SourceDelegateAlias = SourceCallableAlias<Declaration, SourceDelegateAliasAcquisition>;

template<auto& Declaration>
    requires detail::IsSourceDelegate<std::remove_cvref_t<decltype(Declaration)>>::value
constexpr auto ngSourceDelegateAlias() { return SourceDelegateAlias<Declaration>{}; }

} // namespace nijigenerate::compat
