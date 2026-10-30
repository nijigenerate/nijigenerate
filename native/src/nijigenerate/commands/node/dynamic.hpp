#pragma once
#include "dynamic_keys.hpp"
#include "node_commands.hpp"
#include <compat/object_metadata.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/additional_tables.hpp>
namespace nijigenerate::commands::node::dynamic {
template<bool expose> struct ConvertDynamicArguments { compat::RetainedString toType; };
}
namespace nijigenerate::commands {
template<bool E> struct CommandFields<node::dynamic::ConvertDynamicArguments<E>> {
    using A = node::dynamic::ConvertDynamicArguments<E>;
    static constexpr auto descriptors=std::tuple{FieldDescriptor<&A::toType,"toType","destination node type","string",true,!E>{}};
};
}
namespace nijigenerate::commands::node::dynamic {
template<bool expose=true> class ConvertNodeToCommand : public ::nijigenerate::commands::detail::CovariantCommand<
        ConvertNodeToCommand<expose>, ConvertDynamicArguments<expose>> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        ConvertNodeToCommand<expose>, ConvertDynamicArguments<expose>>;
    struct SourceConstructorFrame {
        compat::RetainedString label, description, type;
        static SourceConstructorFrame capture(compat::RetainedString& type);
    };
    explicit ConvertNodeToCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ConvertNodeToCommand<expose>>;
protected:
    void prepareSourceClassInfo() override {
        NativeBase::prepareSourceClassInfo();
        ::nijigenerate::commands::detail::prepareCommandResultClass<ConvertNodeToCommand<expose>>();
    }
public:
    using Result = CreateResult<base::NodeRef>;
    ConvertNodeToCommand(const ConvertNodeToCommand&) = delete;
    ConvertNodeToCommand& operator=(const ConvertNodeToCommand&) = delete;
    ConvertNodeToCommand(ConvertNodeToCommand&&) = delete;
    ConvertNodeToCommand& operator=(ConvertNodeToCommand&&) = delete;
    explicit ConvertNodeToCommand(compat::RetainedString type);
    bool runnable(std::shared_ptr<Context> ctx) override;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx) { return runTyped(ctx); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
std::shared_ptr<node::AddNodeCommandT<false>> ensureAddNodeCommand(compat::RetainedString name,compat::RetainedString suffix={});
std::shared_ptr<node::InsertNodeCommandT<false>> ensureInsertNodeCommand(compat::RetainedString name,compat::RetainedString suffix={});
std::shared_ptr<ConvertNodeToCommand<false>> ensureConvertToCommand(compat::RetainedString type);
void ngInitAddNodeCommands(); void ngInitInsertNodeCommands(); void ngInitConvertNodeCommands();
}

namespace nicxlive::core::d_runtime {
#define NG_CONVERT_NODE_COMMAND_SOURCE_NAMES(Expose) \
    template<> struct SourceObjectTypeNames< \
        ::nijigenerate::commands::node::dynamic::ConvertNodeToCommand<Expose>> { \
        inline static constexpr char className[] = \
            "nijigenerate.commands.node.dynamic.ConvertNodeToCommand!" #Expose ".ConvertNodeToCommand"; \
        inline static constexpr char templateArgumentName[] = \
            "(nijigenerate.commands.node.dynamic.ConvertNodeToCommand!" #Expose ")"; \
    }; \
    template<> struct SourceTypeSpellings< \
        ::nijigenerate::commands::node::dynamic::ConvertNodeToCommand<Expose>> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = \
            "(nijigenerate.commands.node.dynamic.ConvertNodeToCommand!" #Expose ")"; \
        inline static constexpr char unqualifiedArgument[] = "(ConvertNodeToCommand!" #Expose ")"; \
        inline static constexpr char qualifiedElement[] = \
            "nijigenerate.commands.node.dynamic.ConvertNodeToCommand!" #Expose; \
        inline static constexpr char unqualifiedElement[] = "ConvertNodeToCommand!" #Expose; \
    };
NG_CONVERT_NODE_COMMAND_SOURCE_NAMES(true)
NG_CONVERT_NODE_COMMAND_SOURCE_NAMES(false)
#undef NG_CONVERT_NODE_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {
template<bool E> struct CommandTraits<node::dynamic::ConvertNodeToCommand<E>> {
    static constexpr auto attributes=std::tuple{EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<node::dynamic::ConvertDynamicArguments<E>>,Command>;
};

template<bool E> struct SourceCommandConstructor<node::dynamic::ConvertNodeToCommand<E>> {
    template<class... Args> struct Program {
        using C = node::dynamic::ConvertNodeToCommand<E>;
        using String = compat::RetainedString;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<String>, Args...>() &&
                requires(String& type) {
                    { C::SourceConstructorFrame::capture(type) } -> std::same_as<typename C::SourceConstructorFrame>;
                } && SourceExCommandConstructorProgram<node::dynamic::ConvertDynamicArguments<E>>::
                    template preferredSupported<String>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<String>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
}

namespace nijigenerate::commands::node::dynamic {
namespace initialization {
template<class Key> consteval bool supported() {
    if constexpr (std::same_as<Key, AddNodeKey>)
        return ngRuntimeCommandRegistrationSupported<node::AddNodeCommandT<false>,
            decltype(addNodeCommands), Key, compat::RetainedString&, compat::RetainedString&>();
    else if constexpr (std::same_as<Key, InsertNodeKey>)
        return ngRuntimeCommandRegistrationSupported<node::InsertNodeCommandT<false>,
            decltype(insertNodeCommands), Key, compat::RetainedString&, compat::RetainedString&>();
    else if constexpr (std::same_as<Key, ConvertToKey>)
        return ngRuntimeCommandRegistrationSupported<ConvertNodeToCommand<false>,
            decltype(convertNodeCommands), Key, compat::RetainedString&>();
    else return false;
}
}
template<class T> requires (std::same_as<T, AddNodeKey> && initialization::supported<T>())
void ngInitCommands() { ngInitAddNodeCommands(); }
template<class T> requires (std::same_as<T, InsertNodeKey> && initialization::supported<T>())
void ngInitCommands() { ngInitInsertNodeCommands(); }
template<class T> requires (std::same_as<T, ConvertToKey> && initialization::supported<T>())
void ngInitCommands() { ngInitConvertNodeCommands(); }
}

namespace nijigenerate::commands::detail {
template<class Key> requires (std::same_as<Key, node::dynamic::AddNodeKey> ||
    std::same_as<Key, node::dynamic::InsertNodeKey> || std::same_as<Key, node::dynamic::ConvertToKey>)
struct InitializerBody<Key> {
    static consteval bool supported() { return node::dynamic::initialization::supported<Key>(); }
    static void invoke() requires(supported()) { node::dynamic::ngInitCommands<Key>(); }
};
}
