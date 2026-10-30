#include "base.hpp"

#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <utility>

namespace nijigenerate::commands {
namespace {
template<class T> void prepareScopeClass() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<T>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        const auto argument = compat::RetainedString::fromStaticLiteral(Names::templateArgumentName);
        try {
            ngRegisterObjectTypeNames<T>(name, argument);
            ngRegisterObjectClassName(typeid(T), name);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
compat::RetainedString scopeComparisonMessage(const compat::RetainedString& name) {
    try {
        std::string message = "need opCmp for class ";
        message += name.view();
        return compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString scopeEnforceMessage() {
    try {
        return compat::RetainedString(std::string_view("Command scope must not be null"));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
// Used on the editor thread, matching the D __gshared registry contract.
compat::Slice<std::shared_ptr<CommandScope>>& scopeStack() {
    static compat::Slice<std::shared_ptr<CommandScope>> value;
    return value;
}
}
namespace detail {
CommandScopeMap& registeredCommandScopes() {
    static CommandScopeMap value;
    return value;
}
}
CommandScope::~CommandScope() = default;
// Source Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// See nicxlive/licenses/druntime-associative-array.txt for the complete notice.
void CommandScope::prepareSourceClassInfo() const { prepareScopeClass<CommandScope>(); }
void NormalCommandScope::prepareSourceClassInfo() const {
    CommandScope::prepareSourceClassInfo();
    prepareScopeClass<NormalCommandScope>();
}
void GlobalCommandScope::prepareSourceClassInfo() const {
    CommandScope::prepareSourceClassInfo();
    prepareScopeClass<GlobalCommandScope>();
}
compat::RetainedString CommandScope::sourceClassInfoName() const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString CommandScope::toString() { return sourceClassInfoName(); }
std::size_t CommandScope::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool CommandScope::opEquals(std::shared_ptr<CommandScope> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int CommandScope::opCmp(std::shared_ptr<CommandScope>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto name = sourceClassInfoName();
    const auto message = scopeComparisonMessage(name);
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
compat::RetainedString CommandScope::typeName() const {
    return sourceClassInfoName();
}
compat::RetainedString NormalCommandScope::typeName() const {
    return compat::RetainedString::fromStaticView(CommandScopeTypeName<NormalCommandScope>::value);
}
compat::RetainedString GlobalCommandScope::typeName() const {
    return compat::RetainedString::fromStaticView(CommandScopeTypeName<GlobalCommandScope>::value);
}
bool CommandScope::permits(const CommandMetadata& metadata) const {
    if (const auto declared = metadata.scopes.find(typeName()); declared != metadata.scopes.end()) {
        if (compat::ngRequireObject(declared->second)->accepts(this)) return true;
    }
    const auto global = metadata.scopes.find(CommandScopeTypeName<GlobalCommandScope>::value);
    if (global != metadata.scopes.end()) return compat::ngRequireObject(global->second)->accepts(this);
    return false;
}
CommandScopeRegistration::CommandScopeRegistration(std::shared_ptr<CommandScope> registeredScope) {
    if (!registeredScope) throw compat::ThrowableException(
        scopeEnforceMessage(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/base.d"), 284);
    registeredScope_ = std::move(registeredScope);
    scopeStack().append(registeredScope_);
    active_ = true;
}
void CommandScopeRegistration::close() {
    if (!active_) return;
    auto& stack = scopeStack();
    for (auto i = stack.size(); i > 0; --i) {
        if (stack[i - 1] == registeredScope_) {
            const auto prefix = stack.slice(0, i - 1);
            const auto suffix = stack.slice(i, stack.size());
            compat::Slice<std::shared_ptr<CommandScope>> replacement;
            const auto count = prefix.size() + suffix.size();
            if (count) {
                using Scope = std::shared_ptr<CommandScope>;
                using Storage = std::vector<Scope, nicxlive::core::d_runtime::SourceAllocator<Scope>>;
                const auto storage = std::allocate_shared<Storage>(
                    nicxlive::core::d_runtime::SourceAllocator<Storage>{}, count);
                replacement = compat::Slice<Scope>::retainedView(storage, storage->data(), count);
            }
            auto target = replacement.begin();
            for (const auto& scope : prefix) *target++ = scope;
            for (const auto& scope : suffix) *target++ = scope;
            stack = replacement;
            break;
        }
    }
    active_ = false;
}
CommandScopeRegistration::~CommandScopeRegistration() { close(); }
compat::RetainedString CommandScopeRegistration::sourceClassInfoName() const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareScopeClass<CommandScopeRegistration>();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString CommandScopeRegistration::toString() { return sourceClassInfoName(); }
std::size_t CommandScopeRegistration::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool CommandScopeRegistration::opEquals(std::shared_ptr<CommandScopeRegistration> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int CommandScopeRegistration::opCmp(std::shared_ptr<CommandScopeRegistration>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto name = sourceClassInfoName();
    const auto message = scopeComparisonMessage(name);
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
std::shared_ptr<CommandScopeRegistration> ngPushCommandScope(std::shared_ptr<CommandScope> commandScope) {
    return std::allocate_shared<CommandScopeRegistration>(
        nicxlive::core::d_runtime::SourceAllocator<CommandScopeRegistration>{}, std::move(commandScope));
}
std::shared_ptr<CommandScope> ngCurrentCommandScope() {
    if (!scopeStack().empty()) return scopeStack()[scopeStack().size() - 1];
    return ngCommandScope<NormalCommandScope>();
}
compat::Slice<std::shared_ptr<CommandScope>> ngCommandScopeStackSnapshot() {
    compat::Slice<std::shared_ptr<CommandScope>> result;
    result.resize(1);
    result[0] = ngCommandScope<NormalCommandScope>();
    result.append(scopeStack());
    return result;
}
} // namespace nijigenerate::commands
