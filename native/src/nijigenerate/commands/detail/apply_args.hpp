#pragma once

#include "base_ex_args.hpp"
#include <nijigenerate/actions/node_value_action.hpp>
#include <memory>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::detail {

template<class C, class Storage, class Arguments, auto SourceOrigin, std::size_t Index = 0>
consteval bool requireRejectedCommandArgument() {
    using Fields = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    if constexpr (Index == std::tuple_size_v<Fields>) return false;
    else {
        using Expected = typename std::tuple_element_t<Index, Fields>::Type;
        using Actual = std::tuple_element_t<Index, Arguments>;
        if constexpr (acceptsCommandValue<Expected, Actual>())
            return requireRejectedCommandArgument<C, Storage, Arguments, SourceOrigin, Index + 1>();
        else {
            // Native forwarding references are parameter carriers, not Source qualifiers.
            return ngRequireSourceCommandArgumentType<C, Index, std::remove_reference_t<Actual>,
                Expected, SourceOrigin>();
        }
    }
}

template<class C, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr, class... Args>
void applyArgs(std::shared_ptr<C> instance, Args&&... args) {
    using Base = BaseExArgsOf<C, ClassOrigin>;
    using Declared = typename Base::type;
    if constexpr (sizeof...(Args) != 0) {
        if constexpr (sizeof...(Args) != std::tuple_size_v<Declared>) {
            static_assert(ngRequireSourceCommandArgumentCount<C, std::tuple_size_v<Declared>,
                sizeof...(Args), SourceOrigin>(), "Missing defining Source argument-count diagnostic origin");
        } else {
            using Storage = typename Base::Storage;
            constexpr bool compatible = acceptsCommandFields<Storage, Args&...>();
            if constexpr (!compatible) {
                static_assert(requireRejectedCommandArgument<C, Storage, std::tuple<Args&...>, SourceOrigin>(),
                    "Missing defining Source argument-type diagnostic origin");
            } else {
                if (!instance) actions::node::detail::failActionObjectReference();
                // Source auto-ref arguments are named lvalues inside _applyArgs.
                ngAssignCommandFields(static_cast<Storage&>(*instance), args...);
            }
        }
    }
}

} // namespace nijigenerate::commands::detail
