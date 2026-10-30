#include "command.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <functional>
#include <unordered_map>

namespace nijigenerate::commands {
namespace {
compat::RetainedString missingComparisonMessage(const compat::RetainedString& name) {
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
}

// Source Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// See nicxlive/licenses/druntime-associative-array.txt for the complete notice.
void CommandResult::prepareSourceClassInfo() { detail::prepareCommandResultClass<CommandResult>(); }
compat::RetainedString CommandResult::sourceClassInfoName() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString CommandResult::toString() { return sourceClassInfoName(); }
std::size_t CommandResult::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool CommandResult::opEquals(std::shared_ptr<CommandResult> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int CommandResult::opCmp(std::shared_ptr<CommandResult>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto name = sourceClassInfoName();
    const auto message = missingComparisonMessage(name);
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngCommandResultObjectEquals(std::shared_ptr<CommandResult> left, std::shared_ptr<CommandResult> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

void Command::prepareSourceClassInfo() {}
compat::RetainedString Command::sourceClassInfoName() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString Command::toString() { return sourceClassInfoName(); }

std::size_t Command::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool Command::opEquals(std::shared_ptr<Command> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int Command::opCmp(std::shared_ptr<Command>) {
    const auto name = sourceClassInfoName();
    const auto message = missingComparisonMessage(name);
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool ngCommandObjectEquals(std::shared_ptr<Command> left, std::shared_ptr<Command> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

namespace {
using CommandMetadataCache = std::unordered_map<std::type_index, CommandMetadata,
    std::hash<std::type_index>, std::equal_to<std::type_index>,
    nicxlive::core::d_runtime::SourceAllocator<std::pair<const std::type_index, CommandMetadata>>>;
CommandMetadataCache& commandMetadataByTypeName() {
    static CommandMetadataCache value;
    return value;
}
}
namespace detail {
core::settings::SettingsJsonValue CommandResultInitialValue<core::settings::SettingsJsonValue>::get() {
    return {};
}
void registerCommandMeta(std::type_index key, CommandMetadata metadata) {
    commandMetadataByTypeName()[std::move(key)] = std::move(metadata);
}
CommandMetadata lookupCommandMeta(const Command& command) {
    const auto& registry = commandMetadataByTypeName();
    const auto found = registry.find(std::type_index(typeid(command)));
    if (found != registry.end()) return found->second;
    return {};
}
}
bool ngIsPresentationCommand(const std::shared_ptr<Command>& command) {
    return command && detail::lookupCommandMeta(*command).presentationOnly;
}
} // namespace nijigenerate::commands
