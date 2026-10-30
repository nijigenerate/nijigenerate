#pragma once

#include <nijigenerate/commands/automesh/dynamic_key.hpp>
#include <nijigenerate/commands/automesh/dynamic_registry.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <concepts>
#include <memory>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/viewport/vertex/automesh/package.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>
#include "detail/dynamic_progress.hpp"

namespace nijigenerate::commands::automesh::dynamic {

// Each processor retains its concrete command family and shared closure ownership.
namespace detail {
using Processor = viewport::vertex::automesh::AutoMeshProcessor;
compat::RetainedString applyLabel(std::string_view name);
std::shared_ptr<CommandResult> runApply(std::shared_ptr<Command> self, std::shared_ptr<Context> context,
    std::shared_ptr<Processor> explicitProcessor, ParentCanceled parentCanceled,
    bool (*matches)(const std::shared_ptr<Processor>&));
bool canApply(const std::shared_ptr<Context>&);
template<class PT> using ProcessorCommandInfo = viewport::vertex::automesh::meta::AMProcInfo<PT>;
template<class PT> constexpr std::string_view processorName() { return ProcessorCommandInfo<PT>::name; }
compat::RetainedString applyDescription();
}
template<class PT> class ApplyAutoMeshPT : public ExCommand<CommandNoArguments>,
        public std::enable_shared_from_this<ApplyAutoMeshPT<PT>> {
    std::shared_ptr<detail::Processor> explicitProcessor;
    detail::ParentCanceled parentCanceled;
    struct Labels { compat::RetainedString name; compat::RetainedString description; };
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ApplyAutoMeshPT<PT>>;
    static void initializeSourceFields(ApplyAutoMeshPT& target, std::shared_ptr<detail::Processor>& processor,
        detail::ParentCanceled& canceled) {
        SourceCommandInitialization<std::shared_ptr<detail::Processor>,
            std::shared_ptr<detail::Processor>&>::construct(target.explicitProcessor, processor);
        SourceCommandInitialization<detail::ParentCanceled,
            detail::ParentCanceled&>::construct(target.parentCanceled, canceled);
    }
    static Labels labels() {
        auto name=detail::applyLabel(detail::processorName<PT>());
        auto description=detail::applyDescription();
        return {std::move(name),std::move(description)};
    }
    ApplyAutoMeshPT(Labels value,std::shared_ptr<detail::Processor> processor,detail::ParentCanceled canceled)
        : ExCommand<CommandNoArguments>(std::move(value.name),std::move(value.description)) {
        initializeSourceFields(*this, processor, canceled);
    }
protected:
    void prepareSourceClassInfo() override {
        ExCommand<CommandNoArguments>::prepareSourceClassInfo();
        ::nijigenerate::commands::detail::prepareCommandResultClass<ApplyAutoMeshPT<PT>>();
    }
public:
    explicit ApplyAutoMeshPT(std::shared_ptr<detail::Processor> processor = {}, detail::ParentCanceled canceled = {})
        : ApplyAutoMeshPT(labels(),std::move(processor),std::move(canceled)) {}
    template<class LegacyDelegate> requires std::same_as<std::remove_cvref_t<LegacyDelegate>,
        std::shared_ptr<const std::function<bool()>>>
    ApplyAutoMeshPT(std::shared_ptr<detail::Processor> processor, LegacyDelegate&& canceled)
        : ApplyAutoMeshPT(std::move(processor),
            detail::retainParentCanceled(std::forward<LegacyDelegate>(canceled))) {}
    bool runnable(std::shared_ptr<Context> context) override { return detail::canApply(context); }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override {
        return detail::runApply(this->shared_from_this(),context,explicitProcessor,parentCanceled,
            [](const std::shared_ptr<detail::Processor>& processor) {
                return bool(std::dynamic_pointer_cast<PT>(processor));
            });
    }
};

std::shared_ptr<Command> ngEnsureApplyAutoMeshCommand(compat::RetainedStringSlice id);
void ngShutdownAutoMeshJobs();


} // namespace nijigenerate::commands::automesh::dynamic

namespace nijigenerate::commands {
template<class PT> struct CommandTraits<automesh::dynamic::ApplyAutoMeshPT<PT>> {
    static constexpr auto attributes=std::tuple{CommandScopes<NormalCommandScope>{},EffectApply};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};

template<class PT> struct SourceCommandConstructor<automesh::dynamic::ApplyAutoMeshPT<PT>> {
    template<class... Args> struct Program {
        using C = automesh::dynamic::ApplyAutoMeshPT<PT>;
        using Processor = std::shared_ptr<automesh::dynamic::detail::Processor>;
        using Canceled = automesh::dynamic::detail::ParentCanceled;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 2) return false;
            else {
                static_assert(requires {
                    viewport::vertex::automesh::meta::AMSourceAttributes<PT>::attributes;
                    viewport::vertex::automesh::meta::AMSourceAttributes<PT>::identifier;
                }, "Missing defining Source AMProcInfo alias/attribute provider");
                static_assert(requires {
                    typename std::integral_constant<std::size_t,
                        automesh::dynamic::detail::ProcessorCommandInfo<PT>::name.size()>;
                    { C::labels() } -> std::same_as<typename C::Labels>;
                }, "Missing defining Source constructor presentation program");
                static_assert(requires(C& target, Processor& processor, Canceled& canceled) {
                    { C::initializeSourceFields(target, processor, canceled) } -> std::same_as<void>;
                });
                if constexpr (!SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported()
                    || !ngCommandInitializationSupported<Processor, Processor&>()
                    || !ngCommandInitializationSupported<Canceled, Canceled&>()) return false;
                else if constexpr (sizeof...(Args) == 0) return true;
                else if constexpr (sizeof...(Args) == 1)
                    return (ngCommandInitializationSupported<Processor, Args&&>() && ...)
                        && ngCommandParameterCopySupported<Processor, Processor&>();
                else return []<class P, class D>(CommandTypeList<P, D>) {
                    return ngCommandInitializationSupported<Processor, P&&>()
                        && ngCommandInitializationSupported<Canceled, D&&>()
                        && ngCommandParameterCopySupported<Processor, Processor&>()
                        && ngCommandParameterCopySupported<Canceled, Canceled&>();
                }(CommandTypeList<Args...>{});
            }
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            if constexpr (sizeof...(Args) == 0) return std::allocate_shared<C>(allocator);
            else if constexpr (sizeof...(Args) == 1)
                return detail::constructDeclaredSourceCommand<C, std::tuple<Processor>>(
                    allocator, std::forward<Args>(args)...);
            else return detail::constructDeclaredSourceCommand<C, std::tuple<Processor, Canceled>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
}

namespace nijigenerate::commands::automesh::dynamic {
namespace detail {

template<class PT> consteval bool processorRegistrationSupported() {
    if constexpr (!requires { sizeof(PT); }) return false;
    else if constexpr (!std::derived_from<PT, Processor>) return false;
    else if constexpr (!requires {
        viewport::vertex::automesh::meta::AMSourceAttributes<PT>::attributes;
        viewport::vertex::automesh::meta::AMSourceAttributes<PT>::identifier;
    }) return false;
    else return requires {
        // Force the actual metadata producer used by labels and the key.
        typename std::integral_constant<std::size_t, ProcessorCommandInfo<PT>::id.size()>;
        typename std::integral_constant<std::size_t, ProcessorCommandInfo<PT>::name.size()>;
    } && ngRuntimeCommandRegistrationSupported<ApplyAutoMeshPT<PT>,
        AutoMeshApplyCommandMap, AutoMeshKey>();
}

template<class... PT>
consteval bool initializationSupported(compat::TypeSequence<PT...>) {
    return (processorRegistrationSupported<PT>() && ...);
}

} // namespace detail

// The entry and the real processor operations share the same production domain.
// This does not implement an arbitrary D semantic-compilation predicate.
template<class T> requires (std::same_as<T, AutoMeshKey> &&
    detail::initializationSupported(viewport::vertex::automesh::AutoMeshProcessorTypes{}))
void ngInitCommands();
template<> void ngInitCommands<AutoMeshKey>();

} // namespace nijigenerate::commands::automesh::dynamic

namespace nijigenerate::commands::detail {

template<> struct InitializerBody<automesh::dynamic::AutoMeshKey> {
    static consteval bool supported() {
        return automesh::dynamic::detail::initializationSupported(
            viewport::vertex::automesh::AutoMeshProcessorTypes{});
    }
    template<class ActualKey = automesh::dynamic::AutoMeshKey>
        requires (std::same_as<ActualKey, automesh::dynamic::AutoMeshKey> && supported())
    static void invoke() { automesh::dynamic::ngInitCommands<ActualKey>(); }
};

} // namespace nijigenerate::commands::detail

namespace nicxlive::core::d_runtime {
template<class PT> struct SourceObjectTypeNames<
        ::nijigenerate::commands::automesh::dynamic::ApplyAutoMeshPT<PT>> {
    static RetainedString className() {
        return ::nijigenerate::commands::detail::commandTemplateClassName<PT>(
            "nijigenerate.commands.automesh.dynamic", "ApplyAutoMeshPT");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<PT>(
            "nijigenerate.commands.automesh.dynamic", "ApplyAutoMeshPT").qualifiedArgument;
    }
};
template<class PT> struct SourceTypeSpellings<
        ::nijigenerate::commands::automesh::dynamic::ApplyAutoMeshPT<PT>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<PT>(
            "nijigenerate.commands.automesh.dynamic", "ApplyAutoMeshPT");
    }
};
} // namespace nicxlive::core::d_runtime
