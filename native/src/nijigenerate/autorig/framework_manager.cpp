#include "framework.hpp"
#include "detail/framework_runtime.hpp"
#include <compat/default_random.hpp>
#include <cstring>

namespace nijigenerate::autorig::framework {
using namespace detail;
Text ngAutoRigRandomUuid() {
    std::array<unsigned char, 16> bytes;
    auto& random = nicxlive::core::d_runtime::ngDefaultRandom();
    for (std::size_t offset = 0; offset < bytes.size(); offset += sizeof(std::uint32_t)) {
        const auto word = random.front();
        std::memcpy(bytes.data() + offset, &word, sizeof(word));
        random.popFront();
    }
    bytes[8] = (bytes[8] & 0xbf) | 0x80;
    bytes[6] = (bytes[6] & 0x4f) | 0x40;
    constexpr char hex[] = "0123456789abcdef";
    try {
        std::string result(36, '\0');
        std::size_t position = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) result[position++] = '-';
            result[position++] = hex[bytes[i] >> 4];
            result[position++] = hex[bytes[i] & 15];
        }
        return Text(std::move(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
AutoRigSessionManager::AutoRigSessionManager(Text value) {
    prepareSourceClassInfo();
    enforce(!value.empty(), [&] { return Text("AutoRig workspace root is empty"); }, 1164); root = absolutePath(value);
}
void AutoRigSessionManager::registerProcessor(std::shared_ptr<AutoRigProcessor> processor) {
    auto self = shared_from_this();
    enforce(processor && safeId(processor->procId()), [&] { return Text("Invalid AutoRig processor"); }, 1169);
    enforce(!processors.contains(processor->procId()), [&] { return Text("Duplicate AutoRig processor"); }, 1170); processors[processor->procId()] = processor;
}
void AutoRigSessionManager::registerProcessorAlias(Text previous, Text current) {
    enforce(safeId(previous) && !processors.contains(previous) && !processorAliases.contains(previous), [&] { return Text("Invalid or duplicate AutoRig processor alias"); }, 1175);
    enforce(processors.contains(current), [&] { return Text("Unknown AutoRig processor alias target"); }, 1177); processorAliases[previous] = current;
}
compat::Slice<std::shared_ptr<AutoRigProcessor>> AutoRigSessionManager::listProcessors() {
    compat::Slice<std::shared_ptr<AutoRigProcessor>> result;
    for (const auto& [id, processor] : processors) result.append(processor); return result;
}
std::shared_ptr<AutoRigSession> AutoRigSessionManager::create(Text id) {
    auto self = shared_from_this(); return createAt(id, root);
}
std::shared_ptr<AutoRigSession> AutoRigSessionManager::createAt(Text id, Text directory) {
    auto self = shared_from_this();
    return createWithProcessor(processor(id), directory);
}
std::shared_ptr<AutoRigSession> AutoRigSessionManager::createWithProcessor(std::shared_ptr<AutoRigProcessor> processor, Text directory) {
    auto self = shared_from_this();
    enforce(processor && safeId(processor->procId()), [&] { return Text("Invalid AutoRig processor"); }, 1204);
    auto id = ngAutoRigRandomUuid();
    auto session = std::allocate_shared<AutoRigSession>(SourceAllocator<AutoRigSession>{},
        processor, id, joined(directory, id), actionBoundary, editorDispatcher);
    sessions[id] = session; return session;
}
std::shared_ptr<AutoRigSession> AutoRigSessionManager::reopenWithProcessor(std::shared_ptr<AutoRigProcessor> processor, Text id) {
    auto self = shared_from_this();
    auto found = closedSessions.find(id); enforce(found != closedSessions.end() && !sessions.contains(id), [&] { return Text("Unknown closed AutoRig session"); }, 1214);
    auto session = found->second;
    const auto currentProcessorId = compat::ngRequireObject(session)->processorId();
    const auto requestedProcessorId = compat::ngRequireObject(processor)->procId();
    enforce(currentProcessorId == requestedProcessorId,
        [&] { return Text("AutoRig processor identity mismatch"); }, 1216);
    closedSessions.erase(id); sessions[id] = session; return session;
}
std::shared_ptr<AutoRigProcessor> AutoRigSessionManager::processor(Text id) {
    if (const auto alias = processorAliases.find(id); alias != processorAliases.end()) id = alias->second;
    const auto found = processors.find(id); enforce(found != processors.end(), [&] { return Text("Unknown AutoRig processor"); }, 1225); return found->second;
}
std::shared_ptr<AutoRigSession> AutoRigSessionManager::get(Text id) {
    const auto found = sessions.find(id); enforce(found != sessions.end(), [&] { return Text("Unknown AutoRig run"); }, 1233); return found->second;
}
void AutoRigSessionManager::close(Text id) {
    auto self = shared_from_this();
    auto session = get(id); enforce(!compat::ngRequireObject(session)->isBusy(), [&] { return Text("Cannot close a running AutoRig session"); }, 1239);
    closedSessions[id] = session; sessions.erase(id);
}
void AutoRigSessionManager::remove(Text id) {
    auto self = shared_from_this();
    const auto active = sessions.find(id), closed = closedSessions.find(id);
    enforce(active != sessions.end() || closed != closedSessions.end(), [&] { return Text("Unknown AutoRig session"); }, 1247);
    auto session = active != sessions.end() ? active->second : closed->second;
    enforce(!compat::ngRequireObject(session)->isBusy(), [&] { return Text("Cannot delete a running AutoRig session"); }, 1249);
    compat::ngRequireObject(session)->dispose();
    sessions.erase(id); closedSessions.erase(id);
}
void AutoRigSessionManager::disposeAll() {
    auto self = shared_from_this();
    for (const auto& [id, session] : sessions) enforce(!compat::ngRequireObject(session)->isBusy(), [&] { return Text("Cannot dispose a running AutoRig session"); }, 1256);
    for (const auto& [id, session] : closedSessions) enforce(!compat::ngRequireObject(session)->isBusy(), [&] { return Text("Cannot dispose a running AutoRig session"); }, 1257);
    for (const auto& id : keys(sessions)) remove(id);
    for (const auto& id : keys(closedSessions)) remove(id);
}
}
