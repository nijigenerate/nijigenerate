#pragma once
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <compat/object_metadata.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::vertex::define_mesh {

enum class VertexCommand : std::int32_t {
    DefineMesh = 0,
    DefineVertices = 1,
    DefineGrid = 2
};

} // namespace nijigenerate::commands::vertex::define_mesh

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::vertex::define_mesh::VertexCommand> {
    using Enum = commands::vertex::define_mesh::VertexCommand;
    inline static constexpr std::string_view name = "VertexCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::DefineMesh, "DefineMesh"},
        FormatEnumMember<Enum>{Enum::DefineVertices, "DefineVertices"},
        FormatEnumMember<Enum>{Enum::DefineGrid, "DefineGrid"}};
};
} // namespace nijigenerate::core::translation

namespace nijigenerate::commands::vertex::define_mesh {

using CommandTable = ::nijigenerate::commands::CommandTableStorage<VertexCommand>;
extern thread_local CommandTable commands;


// Generated ExCommand field storage, using the canonical retained slice type.
struct DefineMeshArguments {
    compat::Slice<float> vertices;
    compat::Slice<std::uint16_t> indices;
};
struct DefineVerticesArguments {
    compat::Slice<float> vertices;
};
struct DefineGridArguments {
    compat::Slice<float> axisX;
    compat::Slice<float> axisY;
};

} // namespace nijigenerate::commands::vertex::define_mesh

namespace nijigenerate::commands {

template<> struct CommandFields<vertex::define_mesh::DefineMeshArguments> {
    using Storage = vertex::define_mesh::DefineMeshArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::vertices, "vertices", "Flattened [x,y]* vertex coordinates", "float[]">{},
        FieldDescriptor<&Storage::indices, "indices", "Triangle indices (groups of 3)", "ushort[]">{}
    };
};
template<> struct CommandFields<vertex::define_mesh::DefineVerticesArguments> {
    using Storage = vertex::define_mesh::DefineVerticesArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::vertices, "vertices",
            "Flattened [x,y]* control vertex coordinates. For GridDeformer, "
            "vertices must form a complete rectangular grid.",
            "float[]">{}
    };
};
template<> struct CommandFields<vertex::define_mesh::DefineGridArguments> {
    using Storage = vertex::define_mesh::DefineGridArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::axisX, "axisX", "Grid X axis values", "float[]">{},
        FieldDescriptor<&Storage::axisY, "axisY", "Grid Y axis values", "float[]">{}
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::vertex::define_mesh {

class DefineMeshCommand : public ExCommand<DefineMeshArguments>,
    public std::enable_shared_from_this<DefineMeshCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    DefineMeshCommand(compat::Slice<float> vertices, compat::Slice<std::uint16_t> indices);
    DefineMeshCommand(const DefineMeshCommand&) = delete;
    DefineMeshCommand& operator=(const DefineMeshCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DefineMeshCommand>;
    struct SourceConstructorFrame {
        std::pair<compat::RetainedString, compat::RetainedString> text;
        compat::Slice<float> vertices;
        compat::Slice<std::uint16_t> indices;
        static SourceConstructorFrame capture(compat::Slice<float>& vertices,
            compat::Slice<std::uint16_t>& indices);
    };
    explicit DefineMeshCommand(SourceConstructorFrame frame);
};

class DefineVerticesCommand : public ExCommand<DefineVerticesArguments>,
    public std::enable_shared_from_this<DefineVerticesCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    explicit DefineVerticesCommand(compat::Slice<float> vertices);
    DefineVerticesCommand(const DefineVerticesCommand&) = delete;
    DefineVerticesCommand& operator=(const DefineVerticesCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DefineVerticesCommand>;
    struct SourceConstructorFrame {
        std::pair<compat::RetainedString, compat::RetainedString> text;
        compat::Slice<float> vertices;
        static SourceConstructorFrame capture(compat::Slice<float>& vertices);
    };
    explicit DefineVerticesCommand(SourceConstructorFrame frame);
};

class DefineGridCommand : public ExCommand<DefineGridArguments>,
    public std::enable_shared_from_this<DefineGridCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    DefineGridCommand(compat::Slice<float> axisX, compat::Slice<float> axisY);
    DefineGridCommand(const DefineGridCommand&) = delete;
    DefineGridCommand& operator=(const DefineGridCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DefineGridCommand>;
    struct SourceConstructorFrame {
        std::pair<compat::RetainedString, compat::RetainedString> text;
        compat::Slice<float> axisX;
        compat::Slice<float> axisY;
        static SourceConstructorFrame capture(compat::Slice<float>& axisX, compat::Slice<float>& axisY);
    };
    explicit DefineGridCommand(SourceConstructorFrame frame);
};

} // namespace nijigenerate::commands::vertex::define_mesh

namespace nijigenerate::commands {

template<> struct CommandTraits<vertex::define_mesh::DefineMeshCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<vertex::define_mesh::DefineMeshArguments>, Command>;
};
template<> struct CommandTraits<vertex::define_mesh::DefineVerticesCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<vertex::define_mesh::DefineVerticesArguments>, Command>;
};
template<> struct CommandTraits<vertex::define_mesh::DefineGridCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<vertex::define_mesh::DefineGridArguments>, Command>;
};

namespace vertex::define_mesh::detail {

template<class> inline constexpr bool missingArrayConstructorParameter = false;

template<class Element, class Argument> consteval bool arrayParameterSupported() {
    using Value = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<Value, std::nullptr_t> || std::same_as<Value, compat::Slice<Element>>)
        return ::nijigenerate::commands::detail::declaredSourceParametersSupported<
            std::tuple<compat::Slice<Element>>, Argument>();
    else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
    else {
        // A native static array or custom alias-this carrier does not supply
        // its Source array lifetime/conversion merely by being constructible.
        static_assert(missingArrayConstructorParameter<Argument>,
            "Missing defining Source dynamic-array constructor argument program");
    }
}

} // namespace vertex::define_mesh::detail

template<> struct SourceCommandConstructor<vertex::define_mesh::DefineMeshCommand> {
    template<class... Args> struct Program {
        using C = vertex::define_mesh::DefineMeshCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class Vertices, class Indices>(CommandTypeList<Vertices, Indices>) {
                using Base = SourceExCommandConstructorProgram<vertex::define_mesh::DefineMeshArguments>;
                return vertex::define_mesh::detail::arrayParameterSupported<float, Vertices>() &&
                    vertex::define_mesh::detail::arrayParameterSupported<std::uint16_t, Indices>() &&
                    requires(compat::Slice<float>& vertices, compat::Slice<std::uint16_t>& indices) {
                        { C::SourceConstructorFrame::capture(vertices, indices) } ->
                            std::same_as<typename C::SourceConstructorFrame>;
                    } && Base::preferredSupported<compat::Slice<float>, compat::Slice<std::uint16_t>>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C,
                std::tuple<compat::Slice<float>, compat::Slice<std::uint16_t>>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<vertex::define_mesh::DefineVerticesCommand> {
    template<class... Args> struct Program {
        using C = vertex::define_mesh::DefineVerticesCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else {
                using Base = SourceExCommandConstructorProgram<vertex::define_mesh::DefineVerticesArguments>;
                return (vertex::define_mesh::detail::arrayParameterSupported<float, Args>() && ...) &&
                    requires(compat::Slice<float>& vertices) {
                        { C::SourceConstructorFrame::capture(vertices) } ->
                            std::same_as<typename C::SourceConstructorFrame>;
                    } && Base::preferredSupported<compat::Slice<float>>();
            }
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<compat::Slice<float>>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<vertex::define_mesh::DefineGridCommand> {
    template<class... Args> struct Program {
        using C = vertex::define_mesh::DefineGridCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return (vertex::define_mesh::detail::arrayParameterSupported<float, Args>() && ...) &&
                requires(compat::Slice<float>& axisX, compat::Slice<float>& axisY) {
                    { C::SourceConstructorFrame::capture(axisX, axisY) } ->
                        std::same_as<typename C::SourceConstructorFrame>;
                } && SourceExCommandConstructorProgram<vertex::define_mesh::DefineGridArguments>::
                    preferredSupported<compat::Slice<float>, compat::Slice<float>>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C,
                std::tuple<compat::Slice<float>, compat::Slice<float>>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::vertex::define_mesh {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct MeshArguments {
    static auto get() { return std::tuple{compat::Slice<float>{}, compat::Slice<std::uint16_t>{}}; }
};
struct VertexArguments { static auto get() { return std::tuple{compat::Slice<float>{}}; } };
struct GridArguments { static auto get() { return std::tuple{compat::Slice<float>{}, compat::Slice<float>{}}; } };
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<VertexCommand::DefineMesh, DefineMeshCommand, MeshArguments>,
    Required<VertexCommand::DefineVertices, DefineVerticesCommand, VertexArguments>,
    Required<VertexCommand::DefineGrid, DefineGridCommand, GridArguments>>;
}
template<class T> requires (std::same_as<T, VertexCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands();
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<vertex::define_mesh::VertexCommand> {
    static consteval bool supported() {
        return vertex::define_mesh::initialization::Program::supported<vertex::define_mesh::initialization::Table>();
    }
    template<class ActualKey = vertex::define_mesh::VertexCommand>
        requires (std::same_as<ActualKey, vertex::define_mesh::VertexCommand> && supported())
    static void invoke() { vertex::define_mesh::ngInitCommands<ActualKey>(); }
};
}

namespace nicxlive::core::d_runtime {
// Only the original classes and enum have these defining declaration origins.
// ExCommand's generated argument storage is not another Source declaration.
#define NG_VERTEX_DEFINE_TYPE_SPELLINGS(Type, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::vertex::define_mesh::Type> { \
        inline static constexpr bool isNativeObject = Object; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.vertex.define_mesh." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.vertex.define_mesh." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_VERTEX_DEFINE_TYPE_SPELLINGS(DefineMeshCommand, true);
NG_VERTEX_DEFINE_TYPE_SPELLINGS(DefineVerticesCommand, true);
NG_VERTEX_DEFINE_TYPE_SPELLINGS(DefineGridCommand, true);
NG_VERTEX_DEFINE_TYPE_SPELLINGS(VertexCommand, false);
#undef NG_VERTEX_DEFINE_TYPE_SPELLINGS

#define NG_VERTEX_DEFINE_OBJECT_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::vertex::define_mesh::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.vertex.define_mesh." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.vertex.define_mesh." #Type ")"; \
    }
NG_VERTEX_DEFINE_OBJECT_NAMES(DefineMeshCommand);
NG_VERTEX_DEFINE_OBJECT_NAMES(DefineVerticesCommand);
NG_VERTEX_DEFINE_OBJECT_NAMES(DefineGridCommand);
#undef NG_VERTEX_DEFINE_OBJECT_NAMES
} // namespace nicxlive::core::d_runtime
