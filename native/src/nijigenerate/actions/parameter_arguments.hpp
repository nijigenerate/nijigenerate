#pragma once

#include <nijigenerate/compat/scope_exit.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <tuple>
#include <utility>

namespace nijigenerate::actions::parameter::detail {

// Real Source by-value argument storage. No T object exists until its own
// constructor succeeds; each completed argument has exactly one cleanup.
template<class T> class ParameterArgumentCell {
    alignas(T) std::byte storage_[sizeof(T)];
    T* value_ = nullptr;
public:
    ParameterArgumentCell() = default;
    ParameterArgumentCell(const ParameterArgumentCell&) = delete;
    ParameterArgumentCell& operator=(const ParameterArgumentCell&) = delete;
    template<class Value> void construct(Value&& value) {
        value_=::new(static_cast<void*>(storage_)) T(std::forward<Value>(value));
    }
    T& value() { return *value_; }
    void destroy() {
        auto* value=std::exchange(value_,nullptr);
        if(value)std::destroy_at(value);
    }
};

template<class... Types> class ParameterArguments {
    std::tuple<ParameterArgumentCell<Types>...> cells_;
    template<class Values,std::size_t... Index> void construct(Values&& values,std::index_sequence<Index...>) {
        (std::get<Index>(cells_).construct(std::get<Index>(std::forward<Values>(values))),...);
    }
    template<class Body,std::size_t... Index> void apply(Body&& body,std::index_sequence<Index...>) {
        std::invoke(std::forward<Body>(body),std::get<Index>(cells_).value()...);
    }
    template<std::size_t Count> void destroy() {
        if constexpr(Count) {
            compat::ngRunWithScopeExit([&] { std::get<Count-1>(cells_).destroy(); },
                [&] { destroy<Count-1>(); });
        }
    }
public:
    template<class... Values> void construct(Values&&... values) {
        construct(std::forward_as_tuple(std::forward<Values>(values)...),std::index_sequence_for<Types...>{});
    }
    template<class Body> void apply(Body&& body) {
        apply(std::forward<Body>(body),std::index_sequence_for<Types...>{});
    }
    void destroy() { destroy<sizeof...(Types)>(); }
};

template<class... Types,class Body,class... Values>
void withParameterArguments(Body&& body,Values&&... values) {
    ParameterArguments<Types...> arguments;
    compat::ngRunWithScopeExit([&] {
        arguments.construct(std::forward<Values>(values)...);
        arguments.apply(std::forward<Body>(body));
    },[&] { arguments.destroy(); });
}

} // namespace nijigenerate::actions::parameter::detail
