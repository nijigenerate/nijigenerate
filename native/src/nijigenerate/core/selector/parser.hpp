#pragma once

#include "tokenizer.hpp"
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_associative.hpp>
#include <serialization_fields.hpp>
#include <nijigenerate/compat/source_json_enums.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <common/string_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

namespace nijigenerate::core::selector {
class Grammar;
class EvalContext;
class AST;
using GrammarPtr = std::shared_ptr<Grammar>;
using EvalContextPtr = std::shared_ptr<EvalContext>;
using ASTPtr = std::shared_ptr<AST>;

using SelectorString = nijigenerate::compat::RetainedStringSlice;

namespace detail {
template<class Value> using ParserStateMap = SelectorIntegerMap<Value>;
template<class T, class... Args> std::shared_ptr<T> ngCreateParserObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{}, std::forward<Args>(args)...);
}
inline SelectorString ngRetainParserText(std::string_view text) {
    try { return SelectorString(compat::RetainedString(text)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline SelectorString ngRetainParserText(const char* text) {
    try { return SelectorString(compat::RetainedString(text)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

class Grammar : public std::enable_shared_from_this<Grammar> {
public:
    Grammar(const Grammar&) = delete;
    Grammar& operator=(const Grammar&) = delete;
    virtual ~Grammar();
    enum class Type { Token, And, Or, ExOr, Empty, Reference, Repeat, Invalid };
    struct Ref { SelectorString name; bool lazyEval = false; };
    int priority = 0;
    Type type = Type::Token;
    SelectorString name;
    bool forgetUnnamed = false;
    union {
        SelectorArray<GrammarPtr> subGrammars;
        Token token;
        Ref reference;
    };
    int minimumCount = 0;
    Grammar();
    explicit Grammar(SelectorArray<GrammarPtr> subGrammars, SelectorString name = {});
    Grammar(int priority, SelectorArray<GrammarPtr> subGrammars, SelectorString name = {});
    Grammar(int priority, Token token, SelectorString name = {});
    Grammar(int priority, Type type, SelectorString name = {});
    Grammar(int priority, SelectorString refName, bool lazyEval = false, SelectorString name = {});
    Grammar(int priority, Type type, SelectorArray<GrammarPtr> subGrammars, SelectorString name = {});
    virtual GrammarPtr dup();
    virtual SelectorString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(GrammarPtr other);
    virtual int opCmp(GrammarPtr other);
protected:
    virtual void prepareSourceClassInfo();
private:
    enum class ActiveArm { None, Array, Token, Reference };
    ActiveArm activeArm_ = ActiveArm::None;
    void destroyArm() noexcept;
    void constructArm(Type type);
};

class EvalContext : public std::enable_shared_from_this<EvalContext> {
public:
    EvalContext(const EvalContext&) = delete;
    EvalContext& operator=(const EvalContext&) = delete;
    virtual ~EvalContext() = default;
    std::shared_ptr<Scanner> scanner;
    GrammarPtr target;
    bool matched = false;
    Token matchedToken{Token::Type::Invalid};
    SelectorArray<EvalContextPtr> subContexts;
    EvalContext(std::shared_ptr<Scanner> scanner, GrammarPtr target);
    virtual SelectorString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(EvalContextPtr other);
    virtual int opCmp(EvalContextPtr other);
protected:
    virtual void prepareSourceClassInfo();
};

// D associative-array copies share non-null storage; dup copies the table only.
class ASTChildren {
    using Map = nicxlive::core::common::StringKeyMap<ASTPtr>;
    Map values_;
public:
    using Slot = decltype(std::declval<Map&>()[std::declval<compat::RetainedString>()]);
    Slot operator[](const SelectorString& key);
    ASTPtr get(std::string_view key) const;
    ASTPtr at(const SelectorString& key, std::size_t sourceLine = 0) const;
    const Map& values() const;
    ASTChildren dup() const;
};

class AST : public std::enable_shared_from_this<AST> {
public:
    AST(const AST&) = delete;
    AST& operator=(const AST&) = delete;
    virtual ~AST() = default;
    SelectorString name;
    ASTChildren children;
    Token token;
    explicit AST(EvalContextPtr context);
    AST() = default;
    explicit AST(SelectorString name);
    AST(SelectorString name, Token token);
    AST(SelectorString name, ASTChildren children);
    AST(SelectorString name, SelectorArray<ASTPtr> children);
    virtual ASTPtr opIndex(SelectorString name);
    ASTPtr opIndex(const char* name) { return opIndex(detail::ngRetainParserText(name)); }
    ASTPtr opIndex(const std::string& name) { return opIndex(detail::ngRetainParserText(std::string_view(name))); }
    using Iterator = compat::SharedDelegate<int(SelectorString, ASTPtr)>;
    virtual int opApply(Iterator iter);
    int opApply(const std::function<int(const std::string&, const ASTPtr&)>& iter);
    virtual ASTPtr dup();
    virtual SelectorString toString();
    virtual SelectorString pretty(int index);
    virtual std::size_t toHash();
    virtual bool opEquals(ASTPtr other);
    virtual int opCmp(ASTPtr other);
protected:
    virtual void prepareSourceClassInfo();
};

class Parser : public std::enable_shared_from_this<Parser> {
public:
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    virtual ~Parser() = default;
protected:
    virtual void prepareSourceClassInfo();
    Token dummyToken{Token::Type::Invalid};
    GrammarPtr empty = detail::ngCreateParserObject<Grammar>(-1, Grammar::Type::Empty);
    virtual void registerGrammar(SelectorString name, GrammarPtr grammar);
    virtual GrammarPtr _t(SelectorString literal, SelectorString name = {});
    GrammarPtr _t(const char* literal, SelectorString name = {}) {
        return _t(detail::ngRetainParserText(literal), std::move(name));
    }
    virtual GrammarPtr _seq(SelectorArray<GrammarPtr> grammars, SelectorString name = {});
    virtual GrammarPtr _or(SelectorArray<GrammarPtr> grammars, SelectorString name = {});
    virtual GrammarPtr _xor(SelectorArray<GrammarPtr> grammars, SelectorString name = {});
    virtual GrammarPtr _opt(GrammarPtr grammar, SelectorString name = {});
    virtual GrammarPtr _opt(SelectorArray<GrammarPtr> grammar, SelectorString name = {});
    virtual GrammarPtr _id(SelectorString name = {});
    virtual GrammarPtr _d(SelectorString name = {});
    virtual GrammarPtr _str(SelectorString name = {});
    virtual GrammarPtr _ref(SelectorString refName, bool lazyEval = false, SelectorString name = {});
    virtual GrammarPtr _repeat0(SelectorArray<GrammarPtr> grammars, SelectorString name = {});
    virtual GrammarPtr _repeat1(SelectorArray<GrammarPtr> grammars, SelectorString name = {});
    virtual EvalContextPtr eval(EvalContextPtr context);
    virtual GrammarPtr grammarFromId(int id);
public:
    GrammarPtr rootGrammar;
    nicxlive::core::common::StringKeyMap<GrammarPtr> grammars;
    const int EmptyState = -1;
    const int GenerateEmpty = -2;
    nicxlive::core::common::PolicyKeyMap<GrammarPtr, int,
        nicxlive::core::common::ObjectClassKeyPolicy<GrammarPtr>> grammarIDs;
    detail::ParserStateMap<GrammarPtr> reverseIDs;
    using Transitions = detail::ParserStateMap<SelectorArray<int>>;
    detail::ParserStateMap<Transitions> grammarMap;
    enum class StackOp { Push, Pop };
    struct StackOpFrame { StackOp op = StackOp::Push; std::uint64_t numValue = 0; };
    detail::ParserStateMap<StackOpFrame> stackOps;
    struct ASTStack {
        SelectorArray<ASTPtr> values;
        std::shared_ptr<Parser> parser;
        ASTStack() = default;
        explicit ASTStack(std::shared_ptr<Parser> parser);
        void pushItem(ASTPtr ast);
        ASTPtr popFrame(int targetId);
    };
    std::shared_ptr<Tokenizer> tokenizer;
    explicit Parser(std::shared_ptr<Tokenizer> tokenizer);
    virtual ASTPtr parse(SelectorString text);
    virtual ASTPtr parse2(SelectorString text);
    ASTPtr parse(std::string_view text) { return parse(detail::ngRetainParserText(text)); }
    ASTPtr parse2(std::string_view text) { return parse2(detail::ngRetainParserText(text)); }
    virtual void build();
    virtual SelectorString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Parser> other);
    virtual int opCmp(std::shared_ptr<Parser> other);
};

class SelectorParser : public Parser {
protected:
    // Derived allocation-tag constructors initialize fields only; their Source
    // bodies run after this base phase on the genuine complete shared object.
    struct SourceAllocationTag {};
    SelectorParser(SourceAllocationTag, std::shared_ptr<Tokenizer> tokenizer);
    void initializeSource();
    void prepareSourceClassInfo() override;
public:
    inline static const SelectorString ROOT{compat::RetainedString::fromStaticLiteral("query")};
    static std::shared_ptr<SelectorParser> createSource(std::shared_ptr<Tokenizer> tokenizer);
    template<class Derived = SelectorParser, class... Args>
    static std::shared_ptr<Derived> ngCreateDerived(std::shared_ptr<Tokenizer> tokenizer, Args&&... args) {
        static_assert(std::derived_from<Derived, SelectorParser>);
        using Allocator = nicxlive::core::d_runtime::SourceAllocator<Derived>;
        Allocator allocator;
        auto* memory = allocator.allocate(1);
        Derived* object;
        try {
            object = ::new (static_cast<void*>(memory))
                Derived(SourceAllocationTag{}, std::move(tokenizer), std::forward<Args>(args)...);
        } catch (...) {
            allocator.deallocate(memory, 1);
            throw;
        }
        auto result = std::shared_ptr<Derived>(object, [](Derived* value) {
            std::destroy_at(value);
            Allocator{}.deallocate(value, 1);
        }, allocator);
        SelectorParser* receiver = result.get();
        receiver->prepareSourceClassInfo();
        receiver->initializeSource();
        return result;
    }
};
}

namespace nicxlive::core::serde {

template<> struct SourceJsonEnum<::nijigenerate::core::selector::Grammar::Type> {
    template<class Serializer>
    static void serialize(::nijigenerate::core::selector::Grammar::Type value, Serializer& serializer) {
        ::nijigenerate::compat::detail::serializePlainSourceEnum(value, serializer);
    }
};

template<> struct SourceJsonEnum<::nijigenerate::core::selector::Parser::StackOp> {
    template<class Serializer>
    static void serialize(::nijigenerate::core::selector::Parser::StackOp value, Serializer& serializer) {
        ::nijigenerate::compat::detail::serializePlainSourceEnum(value, serializer);
    }
};

template<> struct SourceJsonEnumKey<::nijigenerate::core::selector::Grammar::Type> {
    static d_runtime::RetainedString get(::nijigenerate::core::selector::Grammar::Type value) {
        return ::nijigenerate::compat::detail::enumJsonKey(value);
    }
};

template<> struct SourceJsonEnumKey<::nijigenerate::core::selector::Parser::StackOp> {
    static d_runtime::RetainedString get(::nijigenerate::core::selector::Parser::StackOp value) {
        return ::nijigenerate::compat::detail::enumJsonKey(value);
    }
};

template<> struct SourceJsonFields<::nijigenerate::core::selector::Grammar::Ref> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::Grammar::Ref& value, Serializer& serializer) {
        auto state = serializer.structBegin();
        auto name = value.name;
        serializer.putEscapedKey("name");
        serializer.serializeValue(name);
        auto lazyEval = value.lazyEval;
        serializer.putEscapedKey("lazyEval");
        serializer.serializeValue(lazyEval);
        serializer.structEnd(state);
    }
};

template<> struct SourceJsonFields<::nijigenerate::core::selector::Parser::StackOpFrame> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::Parser::StackOpFrame& value,
            Serializer& serializer) {
        auto state = serializer.structBegin();
        auto op = value.op;
        serializer.putEscapedKey("op");
        serializer.serializeValue(op);
        auto numValue = value.numValue;
        serializer.putEscapedKey("numValue");
        serializer.serializeValue(numValue);
        serializer.structEnd(state);
    }
};

} // namespace nicxlive::core::serde

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::core::selector::Grammar::Ref> {
    using Value = ::nijigenerate::core::selector::Grammar::Ref;
    inline static constexpr std::string_view typeName = "Ref";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("name", offsetof(Value, name), &Value::name),
            ngFormatAggregateMember("lazyEval", offsetof(Value, lazyEval), &Value::lazyEval)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::selector::Parser::StackOpFrame> {
    using Value = ::nijigenerate::core::selector::Parser::StackOpFrame;
    inline static constexpr std::string_view typeName = "StackOpFrame";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("op", offsetof(Value, op), &Value::op),
            ngFormatAggregateMember("numValue", offsetof(Value, numValue), &Value::numValue)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::selector::Parser::ASTStack> {
    using Value = ::nijigenerate::core::selector::Parser::ASTStack;
    inline static constexpr std::string_view typeName = "ASTStack";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("values", offsetof(Value, values), &Value::values),
            ngFormatAggregateMember("parser", offsetof(Value, parser), &Value::parser)};
    }
};

template<> struct FormatEnumTraits<::nijigenerate::core::selector::Grammar::Type> {
    using Enum = ::nijigenerate::core::selector::Grammar::Type;
    inline static constexpr std::string_view name = "Type";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Token, "Token"},
        FormatEnumMember<Enum>{Enum::And, "And"},
        FormatEnumMember<Enum>{Enum::Or, "Or"},
        FormatEnumMember<Enum>{Enum::ExOr, "ExOr"},
        FormatEnumMember<Enum>{Enum::Empty, "Empty"},
        FormatEnumMember<Enum>{Enum::Reference, "Reference"},
        FormatEnumMember<Enum>{Enum::Repeat, "Repeat"},
        FormatEnumMember<Enum>{Enum::Invalid, "Invalid"}};
};
template<> struct FormatEnumTraits<::nijigenerate::core::selector::Parser::StackOp> {
    using Enum = ::nijigenerate::core::selector::Parser::StackOp;
    inline static constexpr std::string_view name = "StackOp";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Push, "Push"},
        FormatEnumMember<Enum>{Enum::Pop, "Pop"}};
};
}

namespace nicxlive::core::d_runtime {
#define NG_PARSER_TYPE_ORIGINS(Type, Qualified, Unqualified) \
    template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(" Qualified ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" Unqualified ")"; \
        inline static constexpr char qualifiedElement[] = Qualified; \
        inline static constexpr char unqualifiedElement[] = Unqualified; \
    }; \
    template<> struct SourceObjectTypeNames<::nijigenerate::core::selector::Type> { \
        inline static constexpr char className[] = Qualified; \
        inline static constexpr char templateArgumentName[] = "(" Qualified ")"; \
    }
NG_PARSER_TYPE_ORIGINS(Grammar, "nijigenerate.core.selector.parser.Grammar", "Grammar");
NG_PARSER_TYPE_ORIGINS(EvalContext, "nijigenerate.core.selector.parser.EvalContext", "EvalContext");
NG_PARSER_TYPE_ORIGINS(AST, "nijigenerate.core.selector.parser.AST", "AST");
NG_PARSER_TYPE_ORIGINS(Parser, "nijigenerate.core.selector.parser.Parser", "Parser");
NG_PARSER_TYPE_ORIGINS(SelectorParser, "nijigenerate.core.selector.parser.SelectorParser", "SelectorParser");
#undef NG_PARSER_TYPE_ORIGINS

#define NG_PARSER_VALUE_TYPE_ORIGIN(Type, Qualified, Unqualified) \
    template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(" Qualified ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" Unqualified ")"; \
        inline static constexpr char qualifiedElement[] = Qualified; \
        inline static constexpr char unqualifiedElement[] = Unqualified; \
    }
NG_PARSER_VALUE_TYPE_ORIGIN(Grammar::Type, "nijigenerate.core.selector.parser.Grammar.Type", "Type");
NG_PARSER_VALUE_TYPE_ORIGIN(Grammar::Ref, "nijigenerate.core.selector.parser.Grammar.Ref", "Ref");
NG_PARSER_VALUE_TYPE_ORIGIN(Parser::StackOp, "nijigenerate.core.selector.parser.Parser.StackOp", "StackOp");
NG_PARSER_VALUE_TYPE_ORIGIN(Parser::StackOpFrame,
    "nijigenerate.core.selector.parser.Parser.StackOpFrame", "StackOpFrame");
NG_PARSER_VALUE_TYPE_ORIGIN(Parser::ASTStack, "nijigenerate.core.selector.parser.Parser.ASTStack", "ASTStack");
#undef NG_PARSER_VALUE_TYPE_ORIGIN

template<> struct SourceTypeSpellings<const ::nijigenerate::core::selector::Grammar::Ref> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(const(nijigenerate.core.selector.parser.Grammar.Ref))";
    inline static constexpr char unqualifiedArgument[] = "(const(Ref))";
    inline static constexpr char qualifiedElement[] = "const(nijigenerate.core.selector.parser.Grammar.Ref)";
    inline static constexpr char unqualifiedElement[] = "const(Ref)";
};

template<> struct SourceTypeSpellings<const ::nijigenerate::core::selector::Parser::StackOpFrame> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(const(nijigenerate.core.selector.parser.Parser.StackOpFrame))";
    inline static constexpr char unqualifiedArgument[] = "(const(StackOpFrame))";
    inline static constexpr char qualifiedElement[] = "const(nijigenerate.core.selector.parser.Parser.StackOpFrame)";
    inline static constexpr char unqualifiedElement[] = "const(StackOpFrame)";
};
}

namespace nijigenerate::compat {

template<> struct SourceReadonlyValueBinding<::nijigenerate::core::selector::Grammar::Ref> {
    using Value = ::nijigenerate::core::selector::Grammar::Ref;
    static constexpr const Value& read(const Value& value) noexcept { return value; }
};

template<> struct SourceReadonlyValueBinding<::nijigenerate::core::selector::Parser::StackOpFrame> {
    using Value = ::nijigenerate::core::selector::Parser::StackOpFrame;
    static constexpr const Value& read(const Value& value) noexcept { return value; }
};

} // namespace nijigenerate::compat

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::Grammar>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::Grammar>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::EvalContext>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::EvalContext>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::AST>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::AST>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::Parser>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::Parser>> {};

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::SelectorParser>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::SelectorParser>> {};

} // namespace nijigenerate::core::translation
