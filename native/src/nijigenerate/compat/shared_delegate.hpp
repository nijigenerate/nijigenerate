#pragma once

#include "native_access_fault.hpp"
#include <compat/source_allocator.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

// Copies retain one callable context, as copies of a captured D delegate do.
template<class Signature> class SharedDelegate;

template<class Result, class... Args>
class SharedDelegate<Result(Args...)> {
    using Function = std::function<Result(Args...)>;
    template<class Callable> struct CallableContext {
        Callable callable;
        Function function;
        template<class Value> explicit CallableContext(Value&& value)
            : callable(std::forward<Value>(value)), function(std::ref(callable)) {}
    };
    template<class Callable> static bool emptyFunction(const Callable&) noexcept { return false; }
    template<class Return, class... Parameters>
    static bool emptyFunction(const std::function<Return(Parameters...)>& callable) noexcept {
        return !callable;
    }
    std::shared_ptr<Function> context_;

public:
    SharedDelegate() noexcept = default;
    SharedDelegate(std::nullptr_t) noexcept {}
    SharedDelegate(const SharedDelegate&) noexcept = default;
    SharedDelegate& operator=(const SharedDelegate&) noexcept = default;
    // Transporting the descriptor must not clear the source delegate.
    SharedDelegate(SharedDelegate&& other) noexcept : context_(other.context_) {}
    SharedDelegate& operator=(SharedDelegate&& other) noexcept {
        context_ = other.context_;
        return *this;
    }

    template<class Callable>
        requires (!std::is_same_v<std::remove_cvref_t<Callable>, SharedDelegate> &&
            std::is_invocable_r_v<Result, std::decay_t<Callable>&, Args...>)
    SharedDelegate(Callable&& callable) {
        using Stored = std::decay_t<Callable>;
        if (emptyFunction(callable)) return;
        if constexpr (std::is_pointer_v<Stored> || std::is_member_pointer_v<Stored>) {
            if (!callable) return;
        }
        if constexpr (std::is_same_v<Stored, Function>) {
            context_ = std::allocate_shared<Function>(nicxlive::core::d_runtime::SourceAllocator<Function>{},
                std::forward<Callable>(callable));
        } else {
            using Context = CallableContext<Stored>;
            auto context = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{},
                std::forward<Callable>(callable));
            context_ = std::shared_ptr<Function>(context, std::addressof(context->function));
        }
    }

    explicit operator bool() const noexcept { return context_ && static_cast<bool>(*context_); }

    Result operator()(Args... args) const {
        // Reentrant code may reset the original descriptor during this call.
        const auto context = context_;
        if (!context || !*context) ngFailNullAccess(false);
        return (*context)(std::forward<Args>(args)...);
    }
};

} // namespace nijigenerate::compat
