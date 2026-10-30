#pragma once
#include <nijigenerate/api/acp/mcp_protocol.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <functional>
#include <memory>
#include <concepts>

namespace nijigenerate::providers::mcp {
using Json=core::settings::SettingsJsonValue;
using Object=core::settings::SettingsJsonObject;
using Array=core::settings::SettingsJsonArray;
using String=compat::RetainedStringSlice;
template<class V> using StringMap=core::selector::SelectorMap<String,V,
    core::selector::SelectorStringHash,core::selector::SelectorStringEqual>;
template<class Signature> class Delegate;
template<class R,class... A> class Delegate<R(A...)> {
    std::shared_ptr<std::function<R(A...)>> function_;
public:
    Delegate()=default;
    Delegate(std::nullptr_t) {}
    Delegate(const Delegate&)=default;
    Delegate& operator=(const Delegate&)=default;
    Delegate(Delegate&& other) noexcept:function_(other.function_) {}
    Delegate& operator=(Delegate&& other) noexcept {function_=other.function_;return *this;}
    template<class F> requires (!std::same_as<std::remove_cvref_t<F>,Delegate> &&
                               std::is_invocable_r_v<R,F&,A...>)
    Delegate(F&& function):function_(std::make_shared<std::function<R(A...)>>(std::forward<F>(function))) {}
    explicit operator bool() const { return bool(function_); }
    R operator()(A...args) const {
        if(!function_) throw NullDelegateError{};
        return (*function_)(std::forward<A>(args)...);
    }
    struct NullDelegateError {};
};
inline String text(std::string_view value) { return compat::RetainedString(value); }
inline Json jsonString(String value) { return core::settings::SettingsString(value); }
}
