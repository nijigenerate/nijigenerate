#include "parser.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <algorithm>
#include <bit>
#include <compat/object_metadata.hpp>

namespace nijigenerate::core::selector {
namespace {
template<class Operation> auto allocateBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString comparisonMessage(compat::RetainedString name) {
    return allocateBytes([&] {
        std::string message = "need opCmp for class ";
        message += name.view();
        return compat::RetainedString(std::move(message));
    });
}
SelectorString retainedText(std::string bytes) {
    return allocateBytes([&] { return SelectorString(compat::RetainedString(std::move(bytes))); });
}
template<class... Pieces> std::string concatenate(const Pieces&... pieces) {
    return allocateBytes([&] {
        std::string result;
        (result.append(pieces), ...);
        return result;
    });
}
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Grammar::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Grammar),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.parser.Grammar"));
        return true;
    }();
    (void)registered;
}
int Grammar::opCmp(std::shared_ptr<Grammar>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void EvalContext::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(EvalContext),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.parser.EvalContext"));
        return true;
    }();
    (void)registered;
}
std::size_t EvalContext::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool EvalContext::opEquals(std::shared_ptr<EvalContext> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int EvalContext::opCmp(std::shared_ptr<EvalContext>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void AST::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AST),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.parser.AST"));
        return true;
    }();
    (void)registered;
}
std::size_t AST::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool AST::opEquals(std::shared_ptr<AST> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int AST::opCmp(std::shared_ptr<AST>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Parser::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Parser),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.parser.Parser"));
        return true;
    }();
    (void)registered;
}
SelectorString Parser::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return SelectorString(nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)));
}
std::size_t Parser::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Parser::opEquals(std::shared_ptr<Parser> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Parser::opCmp(std::shared_ptr<Parser>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void SelectorParser::prepareSourceClassInfo() {
    Parser::prepareSourceClassInfo();
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SelectorParser),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.parser.SelectorParser"));
        return true;
    }();
    (void)registered;
}
namespace {
template<std::size_t N> SelectorString literalName(const char (&text)[N]) {
    return SelectorString(nijigenerate::compat::RetainedString::fromStaticLiteral(text));
}

template<class T, class Read> SelectorArray<T> materializeArray(std::size_t length, Read&& read) {
    SelectorArray<T> result;
    if (length) {
        // These private instantiations contain only retained descriptors/handles.
        if constexpr (std::is_nothrow_default_constructible_v<T>)
            result = SelectorArray<T>(allocateBytes([&] { return std::vector<T>(length); }));
        else result = SelectorArray<T>(std::vector<T>(length));
    }
    for (std::size_t i = 0; i < length; ++i) result[i] = read(i);
    return result;
}

SelectorString grammarText(GrammarPtr value) {
    // std.conv.to!string and std.format both stringify a null class as "null".
    return value ? value->toString() : literalName("null");
}

std::string joinDescriptions(SelectorArray<SelectorString> values, std::string_view separator) {
    return allocateBytes([&] {
        std::string result;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) result += separator;
            result += values[i].view();
        }
        return result;
    });
}

std::string joinGrammars(SelectorArray<GrammarPtr> values, std::size_t count, std::string_view separator) {
    const auto descriptions = materializeArray<SelectorString>(count,
        [&](std::size_t i) { return grammarText(values[i]); });
    return joinDescriptions(descriptions, separator);
}

template<class Map, class Key> auto sourceMapValue(const Map& map, const Key& key, std::size_t line) {
    const auto value = map.findRetained(key);
    if (!value) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/selector/parser.d"), line);
    return *value;
}

template<class Array> auto sourceArrayValue(const Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/selector/parser.d"), line);
    return values[index];
}

template<class Handle> Handle writableObject(Handle value) {
    if (!value) compat::ngFailNullAccess(true);
    return value;
}
}
Grammar::Grammar() : type(Type::Invalid), name(nijigenerate::compat::RetainedString::emptyLiteral()) {
    constructArm(Type::Invalid);
}
void Grammar::constructArm(Type selected) {
    if (selected == Type::Token) {
        std::construct_at(&token);
        activeArm_ = ActiveArm::Token;
    } else if (selected == Type::Reference) {
        std::construct_at(&reference);
        activeArm_ = ActiveArm::Reference;
    } else {
        std::construct_at(&subGrammars);
        activeArm_ = ActiveArm::Array;
    }
}
void Grammar::destroyArm() noexcept {
    switch (activeArm_) {
    case ActiveArm::Array: std::destroy_at(&subGrammars); break;
    case ActiveArm::Token: std::destroy_at(&token); break;
    case ActiveArm::Reference: std::destroy_at(&reference); break;
    case ActiveArm::None: break;
    }
    activeArm_ = ActiveArm::None;
}
Grammar::~Grammar() { destroyArm(); }
std::size_t Grammar::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Grammar::opEquals(GrammarPtr other) {
    return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
}
Grammar::Grammar(SelectorArray<GrammarPtr> values, SelectorString grammarName)
    : Grammar(0, std::move(values), std::move(grammarName)) {}
Grammar::Grammar(int grammarPriority, SelectorArray<GrammarPtr> values, SelectorString grammarName)
    : Grammar(grammarPriority, Type::And, std::move(values), std::move(grammarName)) {}
Grammar::Grammar(int grammarPriority, Token grammarToken, SelectorString grammarName)
    : priority(grammarPriority), type(Type::Token), name(std::move(grammarName)), token(std::move(grammarToken)), activeArm_(ActiveArm::Token) {}
Grammar::Grammar(int grammarPriority, Type grammarType, SelectorString grammarName)
    : priority(grammarPriority), type(grammarType), name(std::move(grammarName)) { constructArm(grammarType); }
Grammar::Grammar(int grammarPriority, SelectorString refName, bool lazyEval, SelectorString grammarName)
    : priority(grammarPriority), type(Type::Reference), name(std::move(grammarName)), reference{std::move(refName), lazyEval}, activeArm_(ActiveArm::Reference) {}
Grammar::Grammar(int grammarPriority, Type grammarType, SelectorArray<GrammarPtr> values, SelectorString grammarName)
    : priority(grammarPriority), type(grammarType), name(std::move(grammarName)), subGrammars(std::move(values)), activeArm_(ActiveArm::Array) {}
GrammarPtr Grammar::dup() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto result = detail::ngCreateParserObject<Grammar>();
    result->name = name;
    result->type = type;
    result->forgetUnnamed = forgetUnnamed;
    result->minimumCount = minimumCount;
    result->priority = priority;
    if (type == Type::And || type == Type::Or || type == Type::ExOr || type == Type::Repeat) {
        const auto children = subGrammars;
        const auto duplicates = materializeArray<GrammarPtr>(children.size(),
            [&](std::size_t i) { return compat::ngRequireObject(children[i])->dup(); });
        result->subGrammars = duplicates;
    } else if (type == Type::Token) {
        result->destroyArm();
        result->constructArm(Type::Token);
        result->token = token;
    } else if (type == Type::Reference) {
        result->destroyArm();
        result->constructArm(Type::Reference);
        result->reference = reference;
    }
    return result;
}
SelectorString Grammar::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto base = name.empty() ? std::string{} : concatenate(name.view(), ": ");
    auto result = [&]() -> std::string {
    switch (type) {
    case Type::Token: return concatenate(base, "\"", token.toString().view(), "\"");
    case Type::And: return concatenate(base, joinGrammars(subGrammars, subGrammars.size(), " "));
    case Type::Or:
    case Type::ExOr:
        if (compat::ngRequireObject(sourceArrayValue(subGrammars, subGrammars.size() - 1, 113))->type == Type::Empty)
            return concatenate(base, "{", joinGrammars(subGrammars, subGrammars.size() - 1, " | "), "}?");
        return concatenate(base, "{", joinGrammars(subGrammars, subGrammars.size(), " | "), "}");
    case Type::Repeat: {
        const auto joined = joinGrammars(subGrammars, subGrammars.size(), " ");
        const auto suffix = minimumCount > 0 ? "+" : "*";
        return concatenate(base, "{", joined, "}", suffix);
    }
    case Type::Empty: return concatenate(base, "---");
    case Type::Reference: return concatenate(base, "-->", reference.name.view());
    case Type::Invalid: return concatenate(base, "<Invalid>");
    }
    return concatenate(base, "<Undefined>");
    }();
    return result.empty() ? SelectorString{} : retainedText(std::move(result));
}
EvalContext::EvalContext(std::shared_ptr<Scanner> value, GrammarPtr grammar)
    : scanner(std::move(value)), target(std::move(grammar)) {}
SelectorString EvalContext::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    std::string body;
    if (!subContexts.empty()) {
        const auto contexts = subContexts;
        const auto descriptions = materializeArray<SelectorString>(contexts.size(),
            [&](std::size_t i) { return compat::ngRequireObject(contexts[i])->toString(); });
        body = joinDescriptions(descriptions, " ");
    } else body = matchedToken.type != Token::Type::Invalid ? concatenate(matchedToken.literal.view())
        : concatenate(compat::ngRequireObject(target)->toString().view());
    const auto title = compat::ngRequireObject(target)->name.isNull() ? std::string{}
        : concatenate(target->name.view(), ":◀");
    return retainedText(concatenate(matched ? "✅" : "❎", title, body, target->name.isNull() ? "" : "▶"));
}
ASTChildren::Slot ASTChildren::operator[](const SelectorString& key) {
    return values_[key.retainedString()];
}
ASTPtr ASTChildren::get(std::string_view key) const {
    const auto found = values_.findRetained(key);
    return found ? *found : ASTPtr{};
}
ASTPtr ASTChildren::at(const SelectorString& key, std::size_t sourceLine) const {
    if (sourceLine) return sourceMapValue(values_, key.view(), sourceLine);
    return values_.get(key.view());
}
const ASTChildren::Map& ASTChildren::values() const {
    return values_;
}
ASTChildren ASTChildren::dup() const {
    ASTChildren result;
    result.values_ = values_.duplicate();
    return result;
}
AST::AST(EvalContextPtr context)
    : name(compat::ngRequireObject(context)->target ? context->target->name : SelectorString{}),
      token(context->matchedToken) {
    const auto contexts = context->subContexts;
    for (auto child : contexts)
        if (compat::ngRequireObject(child)->target && !child->target->name.isNull())
            children[child->target->name] = detail::ngCreateParserObject<AST>(child);
}
AST::AST(SelectorString astName) : name(std::move(astName)) {}
AST::AST(SelectorString astName, Token astToken) : name(std::move(astName)), token(std::move(astToken)) {}
AST::AST(SelectorString astName, ASTChildren astChildren) : name(std::move(astName)), children(std::move(astChildren)) {}
AST::AST(SelectorString astName, SelectorArray<ASTPtr> astChildren) : name(std::move(astName)) {
    for (auto child : astChildren) children[compat::ngRequireObject(child)->name] = child;
}
ASTPtr AST::opIndex(SelectorString key) { return children.get(key.view()); }
int AST::opApply(Iterator iter) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    // Copy the keys before invoking callbacks, matching the D .keys snapshot.
    std::vector<SelectorString> keys;
    allocateBytes([&] { keys.reserve(children.values().size()); });
    children.values().forEachRetained([&](compat::RetainedString key, ASTPtr) {
        keys.emplace_back(std::move(key));
    });
    std::sort(keys.begin(), keys.end(), [](const auto& left, const auto& right) { return left.view() < right.view(); });
    for (const auto& key : keys) {
        const auto child = children.at(key, 201);
        const auto result = iter(key, child);
        if (result) return result;
    }
    return 0;
}
int AST::opApply(const std::function<int(const std::string&, const ASTPtr&)>& iter) {
    return opApply(Iterator([iter](SelectorString key, ASTPtr child) {
        return iter(concatenate(key.view()), child);
    }));
}
ASTPtr AST::dup() {
    auto result = detail::ngCreateParserObject<AST>(name);
    result->children = children.dup();
    result->token = token;
    return result;
}
SelectorString AST::toString() { return pretty(0); }
SelectorString AST::pretty(int index) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto prefix = allocateBytes([&] {
        return std::string(static_cast<std::size_t>(std::max(0, index)), ' ');
    });
    std::string childrenDesc;
    std::vector<SelectorString> keys;
    allocateBytes([&] { keys.reserve(children.values().size()); });
    children.values().forEachRetained([&](compat::RetainedString key, ASTPtr) {
        keys.emplace_back(std::move(key));
    });
    std::sort(keys.begin(), keys.end(), [](const auto& left, const auto& right) { return left.view() < right.view(); });
    const auto descriptions = materializeArray<SelectorString>(keys.size(), [&](std::size_t i) {
        const auto nextIndex = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index) + 2u);
        return compat::ngRequireObject(children.at(keys[i], 226))->pretty(nextIndex);
    });
    if (!descriptions.empty()) childrenDesc = concatenate("\n", joinDescriptions(descriptions, "\n"));
    return retainedText(concatenate(prefix, "◀", name.view(), ": ", token.literal.view(), childrenDesc, "▶"));
}
void Parser::registerGrammar(SelectorString name, GrammarPtr grammar) {
    grammars[name.retainedString()] = grammar;
    writableObject(grammar)->name = std::move(name);
}
GrammarPtr Parser::_t(SelectorString literal, SelectorString name) {
    const auto receiver = compat::ngRequireObject(tokenizer);
    const auto found = receiver->reservedDict.findRetained(literal.view());
    const auto token = !found ? dummyToken : *compat::ngRequireObject(*found);
    return detail::ngCreateParserObject<Grammar>(0, token, std::move(name));
}
GrammarPtr Parser::_seq(SelectorArray<GrammarPtr> values, SelectorString name) {
    auto result = detail::ngCreateParserObject<Grammar>(0, Grammar::Type::And, std::move(values), name);
    if (!name.isNull()) result->forgetUnnamed = true;
    return result;
}
GrammarPtr Parser::_or(SelectorArray<GrammarPtr> values, SelectorString name) {
    auto result = detail::ngCreateParserObject<Grammar>(0, Grammar::Type::Or, std::move(values), name);
    if (!name.isNull()) result->forgetUnnamed = true;
    return result;
}
GrammarPtr Parser::_xor(SelectorArray<GrammarPtr> values, SelectorString name) {
    return detail::ngCreateParserObject<Grammar>(0, Grammar::Type::ExOr, std::move(values), std::move(name));
}
GrammarPtr Parser::_opt(GrammarPtr grammar, SelectorString name) {
    return detail::ngCreateParserObject<Grammar>(0, Grammar::Type::Or,
        SelectorArray<GrammarPtr>{std::move(grammar), detail::ngCreateParserObject<Grammar>(-1, Grammar::Type::Empty)}, std::move(name));
}
GrammarPtr Parser::_opt(SelectorArray<GrammarPtr> values, SelectorString name) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto result = detail::ngCreateParserObject<Grammar>(0, Grammar::Type::Or,
        SelectorArray<GrammarPtr>{_seq(std::move(values)), empty}, name);
    if (!name.isNull()) result->forgetUnnamed = true;
    return result;
}
GrammarPtr Parser::_id(SelectorString name) { return detail::ngCreateParserObject<Grammar>(0, Token(Token::Type::Identifier), std::move(name)); }
GrammarPtr Parser::_d(SelectorString name) { return detail::ngCreateParserObject<Grammar>(0, Token(Token::Type::Digits), std::move(name)); }
GrammarPtr Parser::_str(SelectorString name) { return detail::ngCreateParserObject<Grammar>(0, Token(Token::Type::String), std::move(name)); }
GrammarPtr Parser::_ref(SelectorString refName, bool lazyEval, SelectorString name) {
    return detail::ngCreateParserObject<Grammar>(0, std::move(refName), lazyEval, std::move(name));
}
GrammarPtr Parser::_repeat0(SelectorArray<GrammarPtr> values, SelectorString name) {
    auto result = detail::ngCreateParserObject<Grammar>(0, Grammar::Type::Repeat, std::move(values), name);
    if (!name.isNull()) result->forgetUnnamed = true;
    return result;
}
GrammarPtr Parser::_repeat1(SelectorArray<GrammarPtr> values, SelectorString name) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto result = _repeat0(std::move(values), std::move(name));
    writableObject(result)->minimumCount = 1;
    return result;
}
EvalContextPtr Parser::eval(EvalContextPtr context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto grammar = compat::ngRequireObject(context)->target;
    switch (compat::ngRequireObject(grammar)->type) {
    case Grammar::Type::And:
        context->matched = true;
        for (std::int32_t i = 0; static_cast<std::size_t>(i) < grammar->subGrammars.size();
                i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u)) {
            const auto sub = grammar->subGrammars.at(static_cast<std::size_t>(i));
            auto result = eval(detail::ngCreateParserObject<EvalContext>(context->scanner, sub));
            if (!grammar->forgetUnnamed ||
                    !compat::ngRequireObject(compat::ngRequireObject(result)->target)->name.isNull())
                context->subContexts.push_back(result);
            context->scanner = compat::ngRequireObject(result)->scanner;
            if (!compat::ngRequireObject(result)->matched) { context->matched = false; break; }
        }
        if (grammar->forgetUnnamed && context->subContexts.size() == 1 &&
                compat::ngRequireObject(compat::ngRequireObject(context->subContexts[0])->target)->name.empty()) {
            context->subContexts[0]->target->name = compat::ngRequireObject(context->target)->name;
            context = context->subContexts[0];
        }
        break;
    case Grammar::Type::Repeat: {
        int i = 0;
        while (true) {
            auto thisTry = grammar->dup();
            writableObject(thisTry)->type = Grammar::Type::And;
            thisTry->name = retainedText(allocateBytes([&] { return std::to_string(i); }));
            auto result = eval(detail::ngCreateParserObject<EvalContext>(compat::ngRequireObject(context->scanner)->dup(), thisTry));
            if (compat::ngRequireObject(result)->matched) {
                context->matched = true;
                context->subContexts.push_back(result);
                context->scanner = compat::ngRequireObject(result)->scanner;
            } else break;
            i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u);
        }
        if (i == 0) {
            if (grammar->minimumCount == 0) { context->target = empty; context->matched = true; }
            else context->matched = false;
        }
        break;
    }
    case Grammar::Type::Or:
    case Grammar::Type::ExOr: {
        EvalContextPtr longestMatch;
        const auto alternatives = grammar->subGrammars;
        for (auto sub : alternatives) {
            auto result = eval(detail::ngCreateParserObject<EvalContext>(compat::ngRequireObject(context->scanner)->dup(), sub));
            if (compat::ngRequireObject(result)->matched) {
                if (!longestMatch || compat::ngRequireObject(longestMatch->scanner)->index <
                        compat::ngRequireObject(result->scanner)->index)
                    longestMatch = result;
                if (grammar->type == Grammar::Type::ExOr) break;
            }
        }
        if (longestMatch) {
            if (!grammar->name.isNull()) writableObject(longestMatch->target)->name = grammar->name;
            context = longestMatch;
        } else { context->subContexts.clear(); context->matched = false; }
        break;
    }
    case Grammar::Type::Token: {
        auto next = compat::ngRequireObject(context->scanner)->scan();
        if (grammar->token.opEquals(next)) { context->matchedToken = std::move(next); context->matched = true; }
        break;
    }
    case Grammar::Type::Empty: context->matched = true; break;
    case Grammar::Type::Reference: {
        const auto found = grammars.findRetained(grammar->reference.name.view());
        if (found) {
            const auto referenced = *found;
            const auto duplicate = compat::ngRequireObject(referenced)->dup();
            context = eval(detail::ngCreateParserObject<EvalContext>(context->scanner, duplicate));
        }
        break;
    }
    default: break;
    }
    return context;
}
Parser::Parser(std::shared_ptr<Tokenizer> value) : tokenizer(std::move(value)) {}
GrammarPtr Parser::grammarFromId(int id) {
    const auto found = reverseIDs.findRetained(id);
    return found ? *found : GrammarPtr{};
}
Parser::ASTStack::ASTStack(std::shared_ptr<Parser> value) : parser(std::move(value)) {}
void Parser::ASTStack::pushItem(ASTPtr ast) { values.push_back(std::move(ast)); }
ASTPtr Parser::ASTStack::popFrame(int targetId) {
    if (!compat::ngRequireObject(parser)->stackOps.contains(targetId)) return {};
    const auto g = [&] {
        const auto receiver = parser;
        return compat::ngRequireObject(receiver)->grammarFromId(targetId);
    }();
    auto targetAST = detail::ngCreateParserObject<AST>(g ? g->name : SelectorString{});
    const auto numValue = sourceMapValue(compat::ngRequireObject(parser)->stackOps, targetId, 444).numValue;
    if (values.size() >= numValue) {
        const auto subASTs = values.slice(values.size() - static_cast<std::size_t>(numValue), values.size());
        values = values.slice(0, values.size() - static_cast<std::size_t>(numValue));
        if (subASTs.size() > 1) {
            for (const auto& ast : subASTs)
                if (!compat::ngRequireObject(ast)->name.isNull()) targetAST->children[ast->name] = ast;
        } else if (subASTs.size() == 1) {
            if (!compat::ngRequireObject(g)->name.isNull()) {
                if (!compat::ngRequireObject(subASTs[0])->name.isNull()) targetAST->children[subASTs[0]->name] = subASTs[0];
                else { targetAST = subASTs[0]; targetAST->name = g->name; }
            } else targetAST = subASTs[0];
        }
    }
    return targetAST;
}
ASTPtr Parser::parse(SelectorString text) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    SelectorArray<Token> tokens;
    std::size_t nextPosition;
    const auto sourceTokenizer = tokenizer;
    Tokenizer::tokenizeSource(sourceTokenizer, text, 0, tokens, nextPosition);
    const auto scanner = detail::ngCreateParserObject<Scanner>(tokens);
    const auto context = detail::ngCreateParserObject<EvalContext>(scanner, rootGrammar);
    const auto result = eval(context);
    return detail::ngCreateParserObject<AST>(result);
}
ASTPtr Parser::parse2(SelectorString text) {
    struct LRStackElement {
        int id = 0;
        nijigenerate::compat::RetainedStringSlice literal;
        SelectorArray<LRStackElement> candidates;
        LRStackElement() = default;
        explicit LRStackElement(int i) : id(i) {}
        LRStackElement(int i, nijigenerate::compat::RetainedStringSlice l) : id(i), literal(std::move(l)) {}
    };
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    SelectorArray<Token> tokens;
    std::vector<LRStackElement> lrStack;
    std::size_t nextPosition;
    const auto sourceTokenizer = tokenizer;
    Tokenizer::tokenizeSource(sourceTokenizer, text, 0, tokens, nextPosition);
    ASTStack astStack(compat::ngRequireObject(weak_from_this().lock()));
    auto scanner = detail::ngCreateParserObject<Scanner>(std::move(tokens));
    allocateBytes([&] { lrStack.emplace_back(-1); });
    ASTPtr result;
    [[maybe_unused]] const auto toId = [&](int id) -> SelectorString {
        if (id == EmptyState || id == GenerateEmpty) return literalName("<>");
        const auto found = reverseIDs.findRetained(id);
        if (found) return grammarText(*found);
        static constexpr std::string_view names[] = {"Identifier", "Digits", "Equals", "NotEquals",
            "Greater", "GreaterEq", "Less", "LessEq", "Assign", "LLParen", "RLParen", "LMParen",
            "RMParen", "LSParen", "RSParen", "Comma", "Dot", "Quote", "Space", "Colon", "Plus",
            "Minus", "Multiply", "Division", "Sharp", "Hash", "Dollar", "Question", "String", "Invalid"};
        // Phobos %s on an undeclared enum value prints cast(Type)rawValue.
        auto value = allocateBytes([&] {
            return id >= 0 && id < 30 ? std::string(names[id]) : "cast(Type)" + std::to_string(id);
        });
        return retainedText(std::move(value));
    };
    [[maybe_unused]] const auto dumpStack = [&] {
        for (auto state : lrStack) { (void)state; }
    };
    Token token;
    bool tryEmpty = false;
    bool found = false;
    enum class MatchMode { ConsumeOne, Reduce, GenerateEmpty };
    const auto buildASTFor = [&]<MatchMode mode>(LRStackElement prevState,
                                                LRStackElement state, int nextState) {
        (void)prevState;
        if constexpr (mode == MatchMode::GenerateEmpty) astStack.pushItem(detail::ngCreateParserObject<AST>());
        const auto nextg = grammarFromId(nextState);
        (void)nextg;
        if (const auto g = grammarFromId(state.id)) {
            // The D implementation performs no operation for grammar states here.
        } else if (state.id >= 0) {
            astStack.pushItem(detail::ngCreateParserObject<AST>(SelectorString{},
                Token(static_cast<Token::Type>(state.id), state.literal)));
        }
        if (const auto ast = astStack.popFrame(nextState)) astStack.pushItem(ast);
    };
    const auto transition = [&](int previous, int current) -> std::shared_ptr<const SelectorArray<int>> {
        const auto row = grammarMap.findRetained(previous);
        return row ? row->findRetained(current) : std::shared_ptr<const SelectorArray<int>>{};
    };
    const auto iterate2 = [&] {
        if (lrStack.size() > 1) {
            const auto prevState = lrStack[lrStack.size() - 2];
            const auto state = lrStack.back();
            if (const auto entry = transition(prevState.id, state.id)) {
                found = true;
                const auto next = *entry;
                buildASTFor.template operator()<MatchMode::ConsumeOne>(prevState, state,
                    sourceArrayValue(next, 0, 576));
                lrStack.pop_back();
                lrStack.back().id = sourceArrayValue(next, 0, 579);
                lrStack.back().literal = token.literal;
            }
        }
        if (!lrStack.empty()) {
            const auto prevState = lrStack.back();
            if (const auto entry = transition(prevState.id, EmptyState)) {
                found = true;
                const auto next = *entry;
                buildASTFor.template operator()<MatchMode::Reduce>(prevState, prevState,
                    sourceArrayValue(next, 0, 592));
                lrStack.back().id = sourceArrayValue(next, 0, 594);
                lrStack.back().literal.reset();
            }
        }
    };
    const auto iterate0 = [&] {
        while (!lrStack.empty()) {
            const auto prevState = lrStack.back();
            if (const auto entry = transition(prevState.id, GenerateEmpty)) {
                found = true;
                const auto next = *entry;
                buildASTFor.template operator()<MatchMode::GenerateEmpty>(prevState, prevState,
                    sourceArrayValue(next, 0, 610));
                lrStack.back().id = sourceArrayValue(next, 0, 612);
                lrStack.back().literal.reset();
                break;
            } else break;
        }
    };
    while (!lrStack.empty() && !scanner->isEnd()) {
        found = false;
        if (!tryEmpty) token = scanner->scan();
        else tryEmpty = false;
        allocateBytes([&] { lrStack.emplace_back(static_cast<int>(token.type), token.literal); });
        iterate2();
        if (found) continue;
        lrStack.pop_back();
        tryEmpty = true;
        iterate2();
        if (found) continue;
        iterate0();
    }
    found = true;
    while (found) {
        found = false;
        iterate2();
        if (found) continue;
        iterate0();
    }
    // The original D method never assigns result; preserve its null result.
    return result;
}
void Parser::build() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    int numIDs = 0;
    const auto traverse = [&](auto&& self, GrammarPtr g) -> void {
        grammarIDs[g] = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(numIDs) +
            static_cast<std::uint32_t>(Token::Type::Invalid) + 1u);
        numIDs = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(numIDs) + 1u);
        switch (compat::ngRequireObject(g)->type) {
        case Grammar::Type::And:
        case Grammar::Type::Repeat:
        case Grammar::Type::Or:
        case Grammar::Type::ExOr: {
            const auto children = g->subGrammars;
            for (auto child : children) self(self, child);
            break;
        }
        default: break;
        }
    };
    SelectorArray<GrammarPtr> entries{rootGrammar};
    grammars.forEachRetained([&](compat::RetainedString, GrammarPtr g) { entries.push_back(g); });
    for (auto g : entries) traverse(traverse, g);
    const auto addMap = [&](int state, int last, int nextState) {
        auto row = grammarMap[state];
        auto& transitions = static_cast<Transitions&>(row);
        auto destination = transitions[last];
        static_cast<SelectorArray<int>&>(destination).push_back(nextState);
    };
    const auto traverse2 = [&](auto&& self, GrammarPtr g, int prevState) -> int {
        int state = prevState;
        const int targetId = sourceMapValue(grammarIDs, g, 683);
        int nextState;
        switch (compat::ngRequireObject(g)->type) {
        case Grammar::Type::And: {
            const auto subGrammars = g->subGrammars;
            for (auto sub : subGrammars) {
                nextState = self(self, sub, state);
                (void)grammarMap[state];
                state = nextState;
            }
            if (!g->name.isNull()) stackOps[targetId] = {StackOp::Pop, g->subGrammars.size()};
            addMap(state, EmptyState, targetId);
            break;
        }
        case Grammar::Type::Repeat: {
            if (g->minimumCount == 0) addMap(state, GenerateEmpty, targetId);
            const auto subGrammars = g->subGrammars;
            for (auto sub : subGrammars) {
                nextState = self(self, sub, state);
                (void)grammarMap[state];
                state = nextState;
            }
            if (!g->name.isNull()) stackOps[targetId] = {StackOp::Pop, g->subGrammars.size()};
            const auto repeated = g->dup();
            traverse(traverse, repeated);
            const int repeatedTargetId = sourceMapValue(grammarIDs, repeated, 709);
            addMap(state, GenerateEmpty, targetId);
            const auto repeatedSubGrammars = compat::ngRequireObject(repeated)->subGrammars;
            for (auto sub : repeatedSubGrammars) {
                nextState = self(self, sub, state);
                (void)grammarMap[state];
                state = nextState;
            }
            addMap(state, EmptyState, repeatedTargetId);
            if (!g->name.isNull()) stackOps[repeatedTargetId] = {StackOp::Pop, repeated->subGrammars.size()};
            break;
        }
        case Grammar::Type::Or:
        case Grammar::Type::ExOr: {
            const auto subGrammars = g->subGrammars;
            for (auto sub : subGrammars) {
                const int subId = self(self, sub, state);
                if (!g->name.isNull()) stackOps[targetId] = {StackOp::Pop, 1};
                addMap(subId, EmptyState, targetId);
            }
            break;
        }
        case Grammar::Type::Token:
            addMap(state, static_cast<int>(g->token.type), targetId);
            if (!g->name.isNull()) stackOps[targetId] = {StackOp::Pop, 1};
            break;
        case Grammar::Type::Empty: addMap(state, GenerateEmpty, targetId); break;
        case Grammar::Type::Reference: {
            const auto reference = grammars.findRetained(g->reference.name.view());
            if (reference) {
                const auto referenceGrammar = *reference;
                const auto sub = compat::ngRequireObject(referenceGrammar)->dup();
                traverse(traverse, sub);
                const int subId = self(self, sub, state);
                if (!g->name.isNull()) stackOps[targetId] = {StackOp::Pop, 1};
                addMap(subId, EmptyState, targetId);
            }
            break;
        }
        default: break;
        }
        return targetId;
    };
    traverse2(traverse2, rootGrammar, EmptyState);
    for (const auto& [grammar, id] : grammarIDs) reverseIDs[id] = grammar;
}
SelectorParser::SelectorParser(SourceAllocationTag, std::shared_ptr<Tokenizer> value)
    : Parser(std::move(value)) {}
std::shared_ptr<SelectorParser> SelectorParser::createSource(std::shared_ptr<Tokenizer> value) {
    return ngCreateDerived(std::move(value));
}
void SelectorParser::initializeSource() {
    registerGrammar(literalName("value"), _xor({_id(), _d(), _str()}));
    registerGrammar(literalName("attr"), _repeat1({_t(literalName("[")), _id(literalName("name")),
        _xor({_t(literalName("="))}, literalName("matcher")), _ref(literalName("value")), _t(literalName("]"))}));
    registerGrammar(literalName("args"), _repeat1({
        _ref(literalName("value"), false, literalName("arg")), _opt(_t(literalName(",")))}));
    registerGrammar(literalName("pseudoClass"), _seq({_t(literalName(":")), _id(literalName("name")),
        _opt(_seq({_t(literalName("(")), _ref(literalName("args")), _t(literalName(")"))}, literalName("args")))}));
    registerGrammar(literalName("selectors"), _repeat1({_xor({_t(literalName("#")), _t(literalName("."))}, literalName("kind")),
        _xor({_id(), _str(), _d()}, literalName("name"))}));
    registerGrammar(literalName("typeIdQuery"), _seq({_xor({_id(), _t(literalName("*"))}, literalName("typeId")),
        _opt(_ref(literalName("selectors"))),
        _opt(_ref(literalName("pseudoClass"))), _opt(_ref(literalName("attr")))}));
    registerGrammar(literalName("attrQuery"), _seq({_ref(literalName("selectors")),
        _opt(_ref(literalName("pseudoClass"))), _opt(_ref(literalName("attr")))}));
    registerGrammar(literalName("oneQuery"), _repeat1({_xor({
        _seq({_t(literalName(">"), literalName("kind")),
            _xor({_ref(literalName("typeIdQuery")), _ref(literalName("attrQuery"))})}, literalName("")),
        _xor({_ref(literalName("typeIdQuery")), _ref(literalName("attrQuery"))})})}));
    registerGrammar(literalName("query"), _repeat1({_ref(literalName("oneQuery")), _opt(_t(literalName(",")))}));
    grammars.forEachRetained([&](compat::RetainedString, GrammarPtr grammar) {
        compat::ngRequireObject(grammar)->forgetUnnamed = true;
    });
    rootGrammar = compat::ngRequireObject(sourceMapValue(grammars, ROOT.view(), 784))->dup();
}
}
