#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>
#include <type_traits>

namespace nijigenerate::commands::detail {

template<class T> struct ExCommandStorage {
    static constexpr bool matched = false;
};

template<class Storage> struct ExCommandStorage<ExCommand<Storage>> {
    static constexpr bool matched = true;
    using type = Storage;
};

template<class... Bases> struct FindExCommandStorage;
template<> struct FindExCommandStorage<> { using type = void; };
template<bool Matched, class First, class... Rest> struct FindExCommandStorageStep;
template<class First, class... Rest>
struct FindExCommandStorageStep<true, First, Rest...> : ExCommandStorage<First> {};
template<class First, class... Rest>
struct FindExCommandStorageStep<false, First, Rest...> : FindExCommandStorage<Rest...> {};
template<class First, class... Rest>
struct FindExCommandStorage<First, Rest...>
    : FindExCommandStorageStep<ExCommandStorage<First>::matched, First, Rest...> {};

template<class List> struct FindExCommandStorageInList;

template<class... Bases>
struct FindExCommandStorageInList<CommandTypeList<Bases...>> : FindExCommandStorage<Bases...> {};

template<class Storage> struct ExCommandArgumentDescriptors {
    // Existing descriptors preserve original parameter order, names and wrappers.
    using type = std::remove_cvref_t<decltype(CommandFields<Storage>::descriptors)>;
};

template<> struct ExCommandArgumentDescriptors<void> {
    // No successful extraction alias: BaseExArgsOf supplies the source rejection.
};

template<class S> std::type_identity<S> actualExCommandStorage(const ExCommand<S>*);
std::type_identity<void> actualExCommandStorage(...);

template<class C, auto Origin, bool Supplied = SourceCommandTypeOriginTraits<
    std::remove_cv_t<decltype(Origin)>>::supplied> struct BaseExArgsDiagnosticText {};
template<class C, auto Origin> struct BaseExArgsDiagnosticText<C, Origin, true> {
    inline static constexpr auto sourceFailureText = std::remove_cv_t<decltype(Origin)>::sourceStringOf
        + SourceDiagnosticText{" is not derived from ExCommand!T"};
};
template<HasLiteralCommandClassStringOf C, auto Origin> struct BaseExArgsDiagnosticText<C, Origin, false> {
    inline static constexpr auto sourceFailureText = ngBaseExArgsFailureText<C>();
};

template<class C, class ActualStorage, bool Accepted, auto Origin> struct BaseExArgsResult;
template<class C, class ActualStorage, auto Origin> struct BaseExArgsResult<C, ActualStorage, true, Origin> {
    using Storage = ActualStorage;
    using type = typename ExCommandArgumentDescriptors<Storage>::type;
};
template<class C, class ActualStorage, auto Origin>
struct BaseExArgsResult<C, ActualStorage, false, Origin> : BaseExArgsDiagnosticText<C, Origin> {
    static_assert(ngRequireSourceExCommandBase<C, Origin>(),
        "Missing defining Source class diagnostic producer for BaseExArgsOf rejection");
};

template<class C, auto Origin = nullptr> struct BaseExArgsOf : BaseExArgsResult<C,
    typename decltype(actualExCommandStorage(static_cast<C*>(nullptr)))::type,
    !ExCommandStorage<std::remove_cvref_t<C>>::matched &&
    !std::is_void_v<typename decltype(actualExCommandStorage(static_cast<C*>(nullptr)))::type>, Origin> {
    // D BaseClassesTuple inspects the actual inheritance chain. CommandTraits also
    // describes policy inheritance and need not enumerate every ExCommand base.
};

} // namespace nijigenerate::commands::detail

namespace nijigenerate::commands {
template<class C, auto Origin = nullptr> using BaseExArgsOf = typename detail::BaseExArgsOf<C, Origin>::type;
}
