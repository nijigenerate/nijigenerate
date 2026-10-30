#pragma once

#include "simplephysics_complete.hpp"
#include "simplephysics_action.hpp"
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::node::simplephysics::detail {

// This forwards to the same original private target collector; it owns no second policy.
compat::Slice<std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver>>
collectFloatCommandTargets(std::shared_ptr<Context> context);

template<CommandLiteral ClassName, CommandLiteral Label, CommandLiteral Description,
         CommandLiteral PropertyName, auto Getter, auto Setter>
class SimplePhysicsFloatCommandBody : public ExCommand<PhysicsFloatArguments<Description>>,
    public std::enable_shared_from_this<SimplePhysicsFloatCommandBody<ClassName, Label, Description,
        PropertyName, Getter, Setter>> {
    template<class> friend struct nicxlive::core::d_runtime::SourceObjectTypeNames;
    template<class> friend struct nicxlive::core::d_runtime::SourceTypeSpellings;
    using Base = ExCommand<PhysicsFloatArguments<Description>>;
    struct Text { compat::RetainedString label; compat::RetainedString description; };
    static Text translatedText() {
        auto label = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(Label.value));
        auto description = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(Description.value));
        return {std::move(label), std::move(description)};
    }
    SimplePhysicsFloatCommandBody(Text text, float value)
        : Base(std::move(text.label), std::move(text.description), value) {
        static const bool registered = [] {
            (void)nicxlive::core::d_runtime::ngObjectTemplateArgumentName<SimplePhysicsFloatCommandBody>();
            nicxlive::core::d_runtime::ngRegisterObjectClassName(
                typeid(SimplePhysicsFloatCommandBody), sourceTypeName());
            return true;
        }();
        (void)registered;
    }
    inline static constexpr auto sourceName = []() consteval {
        constexpr std::string_view prefix = "nijigenerate.commands.node.simplephysics.";
        std::array<char, prefix.size() + ClassName.view().size() + 1> result{};
        for (std::size_t i=0; i<prefix.size(); ++i) result[i] = prefix[i];
        for (std::size_t i=0; i<ClassName.view().size(); ++i) result[prefix.size()+i] = ClassName.value[i];
        return result;
    }();
    inline static constexpr auto sourceArgumentName = []() consteval {
        std::array<char, sourceName.size() + 2> result{};
        result[0] = '(';
        for (std::size_t i = 0; i + 1 < sourceName.size(); ++i) result[i + 1] = sourceName[i];
        result[sourceName.size()] = ')';
        return result;
    }();
    inline static constexpr auto sourceUnqualifiedArgumentName = []() consteval {
        std::array<char, ClassName.view().size() + 3> result{};
        result[0] = '(';
        for (std::size_t i = 0; i < ClassName.view().size(); ++i) result[i + 1] = ClassName.value[i];
        result[ClassName.view().size() + 1] = ')';
        return result;
    }();
public:
    explicit SimplePhysicsFloatCommandBody(float value) : SimplePhysicsFloatCommandBody(translatedText(), value) {}
    SimplePhysicsFloatCommandBody(const SimplePhysicsFloatCommandBody&) = delete;
    SimplePhysicsFloatCommandBody& operator=(const SimplePhysicsFloatCommandBody&) = delete;
    static compat::RetainedString sourceTypeName() {
        return compat::RetainedString::fromStaticView({sourceName.data(), sourceName.size()-1});
    }
    template<class ActualCommand>
    static std::shared_ptr<CommandResult> runOn(ActualCommand& command, std::shared_ptr<Context> context) {
        [[maybe_unused]] const auto receiverLifetime = command.weak_from_this().lock();
        using Action = SimplePhysicsValueChangeAction<float, Getter, Setter>;
        core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<Action>(
            ngSimplePhysicsActionArguments([&] {
                const auto property = compat::RetainedString::fromStaticLiteral(PropertyName.value);
                const auto targets = collectFloatCommandTargets(context);
                const auto value = command.value;
                return std::tuple{property, targets, value};
            })));
        return CommandResult::opCall(true);
    }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override {
        return runOn(*this, std::move(context));
    }
};

} // namespace nijigenerate::commands::node::simplephysics::detail

namespace nijigenerate::commands::node::simplephysics {

// A D string mixin's generated identifier is represented by this real named type export.
template<CommandLiteral ClassName, CommandLiteral Label, CommandLiteral Description,
         CommandLiteral PropertyName, auto Getter, auto Setter>
struct SimplePhysicsFloatCommand {
    using type = detail::SimplePhysicsFloatCommandBody<ClassName, Label, Description, PropertyName, Getter, Setter>;
    template<class ActualCommand>
    static std::shared_ptr<CommandResult> runOn(ActualCommand& command, std::shared_ptr<Context> context) {
        return type::runOn(command, std::move(context));
    }
};

} // namespace nijigenerate::commands::node::simplephysics

namespace nijigenerate::commands {

template<CommandLiteral ClassName, CommandLiteral Label, CommandLiteral Description,
         CommandLiteral PropertyName, auto Getter, auto Setter>
struct CommandTraits<node::simplephysics::detail::SimplePhysicsFloatCommandBody<
        ClassName, Label, Description, PropertyName, Getter, Setter>> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<Description>>, Command>;
};

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {

// The real generated class has the identifier supplied by the source mixin,
// independently of the native recipe's six template-argument spellings.
template<::nijigenerate::commands::CommandLiteral ClassName,
         ::nijigenerate::commands::CommandLiteral Label,
         ::nijigenerate::commands::CommandLiteral Description,
         ::nijigenerate::commands::CommandLiteral PropertyName, auto Getter, auto Setter>
struct SourceObjectTypeNames<::nijigenerate::commands::node::simplephysics::detail::SimplePhysicsFloatCommandBody<
        ClassName, Label, Description, PropertyName, Getter, Setter>> {
    using Body = ::nijigenerate::commands::node::simplephysics::detail::SimplePhysicsFloatCommandBody<
        ClassName, Label, Description, PropertyName, Getter, Setter>;
    static RetainedString className() { return Body::sourceTypeName(); }
    static RetainedString templateArgumentName() {
        return RetainedString::fromStaticView(
            {Body::sourceArgumentName.data(), Body::sourceArgumentName.size() - 1});
    }
};

template<::nijigenerate::commands::CommandLiteral ClassName,
         ::nijigenerate::commands::CommandLiteral Label,
         ::nijigenerate::commands::CommandLiteral Description,
         ::nijigenerate::commands::CommandLiteral PropertyName, auto Getter, auto Setter>
struct SourceTypeSpellings<::nijigenerate::commands::node::simplephysics::detail::SimplePhysicsFloatCommandBody<
        ClassName, Label, Description, PropertyName, Getter, Setter>> {
    using Body = ::nijigenerate::commands::node::simplephysics::detail::SimplePhysicsFloatCommandBody<
        ClassName, Label, Description, PropertyName, Getter, Setter>;
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {SourceObjectTypeNames<Body>::templateArgumentName(),
            RetainedString::fromStaticView({Body::sourceUnqualifiedArgumentName.data(),
                Body::sourceUnqualifiedArgumentName.size() - 1}),
            Body::sourceTypeName(), RetainedString::fromStaticLiteral(ClassName.value)};
    }
};

} // namespace nicxlive::core::d_runtime
