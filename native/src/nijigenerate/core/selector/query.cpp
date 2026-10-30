#include "query.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/ext/package.hpp"
#include <common/shared_array.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <bit>
#include <memory>
#include <new>
#include <ranges>
#include <span>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace nijigenerate::core::selector {
namespace {
thread_local std::shared_ptr<ext::ExParameterGroup> dummyRoot;
using RetainedText = nijigenerate::compat::RetainedStringSlice;
using TextOwner = nijigenerate::compat::RetainedString;

template<class Class> void preparePlainQueryClassNames() {
    using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<Class>;
    const auto name = TextOwner::fromStaticLiteral(Names::className);
    nicxlive::core::d_runtime::ngRegisterObjectTypeNames<Class>(name,
        TextOwner::fromStaticLiteral(Names::templateArgumentName));
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class), name);
}

void prepareQueryObjectClassNames() {
    static const bool registered = [] {
        preparePlainQueryClassNames<ResourceCache>();
        preparePlainQueryClassNames<Selector>();
        return true;
    }();
    (void)registered;
}

[[noreturn]] void failQueryObjectComparison(const std::type_info& type) {
    prepareQueryObjectClassNames();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(type);
    const auto message = [&] {
        try {
            std::string text = "need opCmp for class ";
            text += name.view();
            return TextOwner(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message,
        TextOwner::fromStaticLiteral("object.d"), 195);
}

template<std::size_t N> SelectorString staticAstKey(const char (&text)[N]) {
    return SelectorString(TextOwner::fromStaticLiteral(text));
}

// Actual mutable source closure cells, retained if a virtual AST keeps its delegate.
struct BuildQueryState {
    std::shared_ptr<Selector> owner;
    std::shared_ptr<ResourceCache> cache;
    RetainedText lastTypeIdText{TextOwner::emptyLiteral()};
    std::int32_t index = 0;
    SelectorArray<std::shared_ptr<ResourceProcessor>> processors;

    BuildQueryState(std::shared_ptr<Selector> owner, std::shared_ptr<ResourceCache> cache)
        : owner(std::move(owner)), cache(std::move(cache)) {}
};

template<class T, class... Args> std::shared_ptr<T> createBuildObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}

std::shared_ptr<NameFilter> createBuildSelectorNameFilter(const ASTPtr& selector) {
    std::allocator<NameFilter> allocator;
    NameFilter* storage;
    try {
        storage = allocator.allocate(1);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    try {
        std::construct_at(storage,
            compat::ngRequireObject(selector->opIndex(staticAstKey("name")))->token.literal);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    const auto destroy = [](NameFilter* value) noexcept {
        std::destroy_at(value);
        std::allocator<NameFilter>{}.deallocate(value, 1);
    };
    try {
        // shared_ptr invokes this deleter if its own control allocation fails.
        return std::shared_ptr<NameFilter>(storage, destroy);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Callable> AST::Iterator createBuildIterator(Callable&& callable) {
    static_assert(std::is_nothrow_move_constructible_v<std::decay_t<Callable>>);
    try {
        return AST::Iterator(std::forward<Callable>(callable));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> void appendBuildValue(SelectorArray<T>& target, std::type_identity_t<T> value) {
    static_assert(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>);
    try {
        target.push_back(std::move(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

using ParameterKeyPolicy = nicxlive::core::common::ObjectClassKeyPolicy<std::shared_ptr<Parameter>>;
using ParameterKeyHash = ParameterKeyPolicy::Hash;
using ParameterKeyEqual = ParameterKeyPolicy::Equal;

RetainedText duplicateLiteral(const RetainedText& source) {
    // The configured D runtime returns null for a zero-length array .dup.
    if (source.empty()) return {};
    try {
        return RetainedText(TextOwner(std::string(source.view())));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            TextOwner::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            TextOwner::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}

template<class Build> TextOwner buildQueryMetadata(Build build) {
    try {
        return TextOwner(build());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

std::shared_ptr<ext::ExParameterGroup>& detail::ngQueryDummyRoot() { return dummyRoot; }

TextOwner detail::ngQueryTemplateName(std::string_view declaration, TextOwner arguments, bool typeArgument) {
    return buildQueryMetadata([&] {
        std::string text = typeArgument ? "(" : "";
        text += "nijigenerate.core.selector.query.";
        text.append(declaration);
        text += '!';
        text.append(arguments.view());
        if (typeArgument) text += ')';
        else { text += '.'; text.append(declaration); }
        return text;
    });
}

nicxlive::core::d_runtime::SourceTypeSpellingNames detail::ngQueryClassTypeSpellings(
        std::string_view qualifiedElement, std::string_view unqualifiedElement) {
    const auto grouped = [](std::string_view element) {
        return buildQueryMetadata([&] {
            std::string text = "(";
            text.append(element);
            text += ')';
            return text;
        });
    };
    return {grouped(qualifiedElement), grouped(unqualifiedElement),
        buildQueryMetadata([&] { return std::string(qualifiedElement); }),
        buildQueryMetadata([&] { return std::string(unqualifiedElement); })};
}

nicxlive::core::d_runtime::SourceTypeSpellingNames detail::ngQueryTemplateTypeSpellings(
        std::string_view declaration, TextOwner arguments) {
    const auto qualified = buildQueryMetadata([&] {
        std::string text = "nijigenerate.core.selector.query.";
        text.append(declaration);
        text += '!';
        text.append(arguments.view());
        return text;
    });
    const auto unqualified = buildQueryMetadata([&] {
        std::string text(declaration);
        text += '!';
        text.append(arguments.view());
        return text;
    });
    return ngQueryClassTypeSpellings(qualified.view(), unqualified.view());
}

void detail::ngRegisterQueryTemplateClass(const std::type_info& type, std::string_view declaration,
    TextOwner arguments) {
    const auto name = ngQueryTemplateName(declaration, arguments, false);
    nicxlive::core::d_runtime::ngRegisterObjectClassName(type, name);
}

TextOwner detail::ngQueryStringArgument(std::string_view text) {
    return nicxlive::core::d_runtime::ngSourceStringValueArgument(text, '\0', false);
}

TextOwner detail::ngQueryConstBasicArgument(TextOwner element, bool argument) {
    return buildQueryMetadata([&] {
        std::string result = argument ? "(const(" : "const(";
        result.append(element.view());
        result += argument ? "))" : ")";
        return result;
    });
}

TextOwner detail::ngQueryFilterArguments(std::string_view name, TextOwner type, TextOwner op) {
    const auto nameArgument = nicxlive::core::d_runtime::ngSourceStringValueArgument(name, '\0', false);
    return buildQueryMetadata([&] {
        std::string text = "(";
        text.append(nameArgument.view());
        text += ", ";
        text.append(type.view());
        text += ", ";
        text.append(op.view());
        text += ')';
        return text;
    });
}

TextOwner detail::ngQueryWalkerArguments(TextOwner source, TextOwner target, bool direct) {
    return buildQueryMetadata([&] {
        std::string text = "(";
        text.append(source.view());
        text += ", ";
        text.append(target.view());
        text += direct ? ", true)" : ", false)";
        return text;
    });
}

// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0.
// Retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString detail::ngProcessorObjectClassName(const std::type_info& type) {
    static const bool registered = [] {
        preparePlainQueryClassNames<MarkerProcessor>();
        preparePlainQueryClassNames<PuppetWalker>();
        return true;
    }();
    (void)registered;
    return nicxlive::core::d_runtime::ngObjectClassName(type);
}
[[noreturn]] void detail::ngFailProcessorObjectComparison(const std::type_info& type) {
    const auto name = ngProcessorObjectClassName(type);
    const auto message = buildQueryMetadata([&] {
        std::string text = "need opCmp for class ";
        text += name.view();
        return text;
    });
    throw compat::ThrowableException(message,
        TextOwner::fromStaticLiteral("object.d"), 195);
}
compat::RetainedString ResourceCache::toString() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareQueryObjectClassNames();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ResourceCache::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int ResourceCache::opCmp(std::shared_ptr<ResourceCache>) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    failQueryObjectComparison(typeid(*this));
}
bool ResourceCache::opEquals(std::shared_ptr<ResourceCache> other) {
    return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
}
compat::RetainedString Selector::toString() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareQueryObjectClassNames();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Selector::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int Selector::opCmp(std::shared_ptr<Selector>) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    failQueryObjectComparison(typeid(*this));
}
bool Selector::opEquals(std::shared_ptr<Selector> other) {
    return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
}
ResourceArray detail::ngProcessNodeBindingWalker(ResourceArray targets,
    std::shared_ptr<ResourceCache>& cache, bool& armedOnly) {
    if (!project::incActivePuppet()) return {};
    ResourceArray result;
    const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
    nicxlive::core::common::SharedArray<std::shared_ptr<ParameterBinding>> bindings;
    if (!armedOnly) {
        for (auto param : parameters) {
            const auto parameterBindings = compat::ngRequireObject(param)->bindings;
            for (auto binding : parameterBindings) {
                try {
                    bindings.push_back(binding);
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
                }
            }
        }
    } else if (project::incArmedParameter()) {
        bindings = compat::ngRequireObject(project::incArmedParameter())->bindings;
    }
    for (auto t : targets) {
        if (!std::dynamic_pointer_cast<Proxy<Node>>(t)) continue;
        const auto target = to<Node>(t);
        for (auto binding : bindings) {
            if (nicxlive::core::ngResourceObjectEquals(
                    compat::ngRequireObject(binding)->getTarget().target.lock(), target)) {
                const auto sourceCache = cache;
                auto proxy = ResourceCache::createSource(sourceCache, binding, [&](ResourcePtr proxy) {
                    if (!proxy) compat::ngFailNullAccess(true);
                    proxy->index = result.size();
                    proxy->source = t;
                });
                appendBuildValue(result, std::move(proxy));
            }
        }
    }
    return result;
}

ResourceArray detail::ngProcessNodeParameterWalker(ResourceArray targets,
    std::shared_ptr<ResourceCache>& cache) {
    if (!project::incActivePuppet()) return {};
    ResourceArray result;
    const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
    for (auto t : targets) {
        if (!std::dynamic_pointer_cast<Proxy<Node>>(t)) continue;
        const auto target = to<Node>(t);
        for (auto param : parameters) {
            const auto parameterBindings = compat::ngRequireObject(param)->bindings;
            for (auto binding : parameterBindings) {
                if (nicxlive::core::ngResourceObjectEquals(
                        compat::ngRequireObject(binding)->getTarget().target.lock(), target)) {
                    const auto sourceCache = cache;
                    auto proxy = ResourceCache::createSource(sourceCache, param, [&](ResourcePtr proxy) {
                        if (!proxy) compat::ngFailNullAccess(true);
                        proxy->source = t;
                        proxy->index = result.size();
                    });
                    appendBuildValue(result, std::move(proxy));
                    break;
                }
            }
        }
    }
    return result;
}

PuppetWalker::PuppetWalker(std::shared_ptr<ResourceCache> value) : NodeDescendantsWalker(std::move(value)) {}
ResourceArray PuppetWalker::process(ResourceArray targets) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (!project::incActivePuppet()) return {};
    auto& dummyRoot = detail::ngQueryDummyRoot();
    if (!dummyRoot) {
        dummyRoot = createBuildObject<ext::ExParameterGroup>();
        compat::ngRequireObject(dummyRoot)->setName(ResourceString::fromStaticLiteral("Parameters"));
    }
    const auto parameterCache = cache;
    auto paramRoot = ResourceCache::createSource(parameterCache, dummyRoot);
    paramRoot->index = 0;
    const auto rootCache = cache;
    auto root = ResourceCache::createSource(rootCache, compat::ngRequireObject(project::incActivePuppet())->root);
    try {
        targets = ResourceArray{root};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    root->index = 1;
    const auto exPuppet = std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    ResourceArray result;
    nicxlive::core::common::ResourceKeyMap<std::shared_ptr<Parameter>, bool,
        ParameterKeyHash, ParameterKeyEqual> paramAdded;
    if (exPuppet) {
        const auto groups = exPuppet->groups;
        for (auto group : groups) {
            const auto groupCache = cache;
            auto gres = ResourceCache::createSource(groupCache, group);
            gres->index = result.size();
            appendBuildValue(result, gres);
            gres->source = paramRoot;
            gres->explicitSelection = true;
            const auto children = compat::ngRequireObject(group)->children;
            for (auto param : children) {
                const auto sourceCache = cache;
                auto resource = ResourceCache::createSource(sourceCache, param);
                resource->index = result.size();
                appendBuildValue(result, resource);
                resource->source = gres;
                paramAdded[param] = true;
            }
        }
        const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
        for (auto param : parameters) {
            if (!paramAdded.contains(param)) {
                const auto sourceCache = cache;
                auto resource = ResourceCache::createSource(sourceCache, param);
                resource->index = result.size();
                appendBuildValue(result, resource);
                resource->source = paramRoot;
            }
        }
    } else {
        const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
        for (auto param : parameters) {
            const auto sourceCache = cache;
            auto resource = ResourceCache::createSource(sourceCache, param);
            resource->index = result.size();
            appendBuildValue(result, resource);
            resource->source = paramRoot;
        }
    }
    for (const auto& node : NodeDescendantsWalker::process(targets)) appendBuildValue(result, node);
    return result;
}

Selector::Selector() : tokenizer(createBuildObject<Tokenizer>()),
    parser([&] {
        try {
            return SelectorParser::createSource(tokenizer);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }()) {}
void Selector::build(SelectorString text) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto rootAST = compat::ngRequireObject(parser)->parse(text);
    auto cache = createBuildObject<ResourceCache>();
    compat::ngRequireObject(rootAST)->opApply(createBuildIterator([this, owner, cache](SelectorString, ASTPtr outerQuery) {
        const auto state = createBuildObject<BuildQueryState>(owner, cache);
        auto& i = state->index;
        auto& andProcessors = state->processors;
        const auto queryAST = compat::ngRequireObject(outerQuery)->opIndex(staticAstKey("oneQuery"));
        if (!queryAST) return 0;
        queryAST->opApply(createBuildIterator([state](SelectorString, ASTPtr filterAST) {
            auto& lastTypeIdText = state->lastTypeIdText;
            auto& i = state->index;
            auto& andProcessors = state->processors;
            const auto cache = state->cache;
            ASTPtr query;
            if (compat::ngRequireObject(filterAST)->opIndex(staticAstKey("typeIdQuery"))) {
                query = filterAST->opIndex(staticAstKey("typeIdQuery"));
            } else if (filterAST->opIndex(staticAstKey("attrQuery"))) {
                query = filterAST->opIndex(staticAstKey("attrQuery"));
            }
            if (!query) return 0;
            const auto typeId = query->opIndex(staticAstKey("typeId"));
            const auto selectors = query->opIndex(staticAstKey("selectors"));
            const auto pseudoClass = query->opIndex(staticAstKey("pseudoClass"));
            const auto attrs = query->opIndex(staticAstKey("attr"));
            const auto typeIdText = typeId ? typeId->token.literal : RetainedText(TextOwner::emptyLiteral());
            const auto typeIdStr = typeIdText.view();
            const auto lastTypeIdStr = lastTypeIdText.view();
            const bool isPrevNode = Node::inHasNodeTypeRetained(lastTypeIdText.retainedString()) ||
                lastTypeIdStr.empty() || lastTypeIdStr == "*";
            const bool isNode = Node::inHasNodeTypeRetained(typeIdText.retainedString()) ||
                typeIdStr.empty() || typeIdStr == "*";
            if (isPrevNode && isNode) {
                if (i > 0) {
                    if (filterAST->opIndex(staticAstKey("kind")) &&
                            compat::ngRequireObject(filterAST->opIndex(staticAstKey("kind")))->token.equals(">"))
                        appendBuildValue(andProcessors, createBuildObject<NodeChildWalker>(cache));
                    else appendBuildValue(andProcessors, createBuildObject<NodeDescendantsWalker>(cache));
                }
            } else if (isPrevNode) {
                if (i > 0 && typeIdStr == "Parameter") {
                    appendBuildValue(andProcessors, createBuildObject<ResourceWalker<Node, Parameter, true>>(cache));
                } else if (typeIdStr == "Binding") {
                    bool armedOnly = false;
                    if (pseudoClass && compat::ngRequireObject(
                            pseudoClass->opIndex(staticAstKey("name")))->token.equals("active"))
                        armedOnly = true;
                    appendBuildValue(andProcessors,
                        createBuildObject<ResourceWalker<Node, ParameterBinding, true>>(cache, armedOnly));
                }
            } else if (typeIdStr == "Binding") {
                appendBuildValue(andProcessors, createBuildObject<ParameterChildWalker>(cache));
            }
            if (typeIdStr != "*" && typeId)
                appendBuildValue(andProcessors, createBuildObject<TypeIdFilter>(typeId->token.literal));
            if (selectors) selectors->opApply(createBuildIterator([state](SelectorString, ASTPtr selector) {
                auto& andProcessors = state->processors;
                if (compat::ngRequireObject(selector)->opIndex(staticAstKey("kind")) &&
                        compat::ngRequireObject(selector->opIndex(staticAstKey("kind")))->token.equals(".")) {
                    appendBuildValue(andProcessors, createBuildSelectorNameFilter(selector));
                } else if (selector->opIndex(staticAstKey("kind")) &&
                        compat::ngRequireObject(selector->opIndex(staticAstKey("kind")))->token.equals("#")) {
                    try {
                        const auto copied = duplicateLiteral(compat::ngRequireObject(
                            selector->opIndex(staticAstKey("name")))->token.literal);
                        auto value = copied.view();
                        appendBuildValue(andProcessors, createBuildObject<UUIDFilter>(ngParseSelectorUInt(value)));
                    } catch (const SelectorConvException&) {}
                }
                return 0;
            }));
            if (attrs) attrs->opApply(createBuildIterator([state](SelectorString, ASTPtr attrItem) {
                auto& andProcessors = state->processors;
                const auto an = compat::ngRequireObject(attrItem)->opIndex(staticAstKey("name"));
                const auto av = attrItem->opIndex(staticAstKey("value"));
                if (!an || !av) return 0;
                const auto attrNameText = an->token.literal;
                const auto attrName = attrNameText.view();
                const auto copied = duplicateLiteral(av->token.literal);
                auto value = copied.view();
                if (attrName == "name") appendBuildValue(andProcessors, createBuildObject<NameFilter>(copied));
                else if (attrName == "uuid") {
                    try { appendBuildValue(andProcessors, createBuildObject<UUIDFilter>(ngParseSelectorUInt(value))); }
                    catch (const SelectorConvException&) {}
                } else if (attrName == "typeId") appendBuildValue(andProcessors, createBuildObject<TypeIdFilter>(copied));
                return 0;
            }));
            appendBuildValue(andProcessors, createBuildObject<MarkerProcessor>());
            lastTypeIdText = typeIdText;
            i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u);
            return 0;
        }));
        if (i > 0) {
            using ProcessorPtr = std::shared_ptr<ResourceProcessor>;
            const auto prefixed = [&] {
                try {
                    return SelectorArray<ProcessorPtr>{createBuildObject<PuppetWalker>(cache)};
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            const std::array<std::span<const ProcessorPtr>, 2> parts{
                std::span<const ProcessorPtr>{prefixed.data(), prefixed.size()},
                std::span<const ProcessorPtr>{andProcessors.data(), andProcessors.size()}};
            const auto joined = parts | std::views::join;
            try {
                andProcessors = SelectorArray<ProcessorPtr>(std::vector<ProcessorPtr>(joined.begin(), joined.end()));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }
        if (!andProcessors.empty()) appendBuildValue(processors, andProcessors);
        return 0;
    }));
}
ResourceArray Selector::run() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    ResourceArray result;
    const auto branches = processors;
    for (std::size_t branchIndex = 0; branchIndex < branches.size(); ++branchIndex) {
        const auto andProcessors = branches[branchIndex];
        ResourceArray nodes;
        for (std::size_t index = 0; index < andProcessors.size(); ++index) {
            const auto processor = andProcessors[index];
            nodes = compat::ngRequireObject(processor)->process(nodes);
        }
        try {
            result.appendRange(nodes);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                TextOwner::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
    }
    return result;
}
}
