#include "autorig_json_storage.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <compat/source_allocator.hpp>
#include <charconv>
#include <initializer_list>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace autorig_json_storage_test {
namespace {
namespace compat = nijigenerate::compat;
namespace framework = nijigenerate::autorig::framework;
using Text = compat::RetainedString;
using Json = nijigenerate::core::settings::SettingsJsonValue;
using nicxlive::core::d_runtime::SourceAllocator;

template<class F> auto allocateValue(F&& operation) {
    try {
        return std::forward<F>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void require(bool condition, std::size_t line, Text message = Text::fromStaticLiteral("Assertion failure")) {
    if (!condition) throw compat::ngStaticError<compat::AssertError>(std::move(message),
        Text::fromStaticLiteral("tests/autorig_json_storage.d"), line);
}

template<class Integer> std::string decimal(Integer value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return allocateValue([&] { return std::string(buffer, result.ptr); });
}

void writeLine(std::initializer_list<std::string_view> parts) {
    const auto bytes = allocateValue([&] {
        std::string result;
        for (const auto part : parts) result.append(part);
        return result;
    });
    nijigenerate::providers::mcp::stdio::standardOutput().writeln(bytes);
}

class SnapshotProcessor : public framework::AutoRigProcessor {
protected:
    void prepareSourceClassInfo() override {
        framework::AutoRigProcessor::prepareSourceClassInfo();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SnapshotProcessor),
            Text::fromStaticLiteral("autorig_json_storage.SnapshotProcessor"));
    }
public:
    Json payload;
    SnapshotProcessor() { prepareSourceClassInfo(); }
    Text procId() override { return Text::fromStaticLiteral("json-storage-test"); }
    Text displayName() override { return Text::fromStaticLiteral("JSON storage test"); }
    compat::Slice<framework::AutoRigTaskSpec> tasks() override {
        compat::Slice<framework::AutoRigTaskSpec> result;
        for (int index = 0; index < 12; ++index) {
            framework::AutoRigTaskSpec spec;
            const auto number = decimal(index);
            spec.id = allocateValue([&] { return Text("stage-" + number); });
            spec.outputs = compat::Slice<framework::AutoRigPortSpec>{
                {Text::fromStaticLiteral("state"), framework::AutoRigValueKind::Json}};
            result.append(spec);
        }
        return result;
    }
    void executeTask(Text, std::shared_ptr<framework::AutoRigTaskContext> context) override {
        context->publishJson(Text::fromStaticLiteral("state"), payload);
    }
};

void createAndRemove(std::shared_ptr<framework::AutoRigSessionManager> manager,
    std::shared_ptr<SnapshotProcessor> processor) {
    auto session = manager->create(processor->procId());
    for (const auto spec : processor->tasks()) session->execute(spec.id.retainedString());
    manager->remove(session->id());
}
} // namespace

void ngMain(compat::Slice<Text> args) {
    require(args.size() == 3, 38,
        Text::fromStaticLiteral("Pass old|packed|cycles and a real PSD cloud JSON path"));
    auto processor = std::allocate_shared<SnapshotProcessor>(SourceAllocator<SnapshotProcessor>{});
    processor->payload = nijigenerate::autorig::json::ngParseAutoRigJson(
        compat::phobos::ngReadUtf8Text(args[2]));
    processor->payload.sourceIndexAssignSlot(nijigenerate::core::settings::SettingsString(
        Text::fromStaticLiteral("precision"))) = Json(1.2345678901234567);
    if (args[1].view() == "cycles") {
        auto manager = std::allocate_shared<framework::AutoRigSessionManager>(
            SourceAllocator<framework::AutoRigSessionManager>{}, Text::fromStaticLiteral("out/json-storage-no-files"));
        manager->registerProcessor(processor);
        for (int index = 0; index < 4; ++index) {
            createAndRemove(manager, processor);
            // Source line 55 compares GC statistics. No GC domain exists here;
            // retain the real deletion cycle without substituting another metric.
            writeLine({"Delete cycle ", decimal(index + 1), ": GC measurement unavailable (no GC)"});
        }
        return;
    }
    compat::Slice<framework::AutoRigValue> old;
    std::shared_ptr<framework::AutoRigSession> session;
    if (args[1].view() == "old") {
        for (int index = 0; index < 12; ++index)
            old.append(framework::ngCopyAutoRigValue(framework::AutoRigValue::jsonValue(processor->payload)));
    } else {
        auto manager = std::allocate_shared<framework::AutoRigSessionManager>(
            SourceAllocator<framework::AutoRigSessionManager>{}, Text::fromStaticLiteral("out/json-storage-no-files"));
        manager->registerProcessor(processor);
        session = manager->create(processor->procId());
        for (const auto spec : processor->tasks()) session->execute(spec.id.retainedString());
        const auto artifact = session->task(Text::fromStaticLiteral("stage-0")).artifacts[0];
        require(artifact.storagePath.empty() && artifact.byteLength() > 0, 69);
        const auto info = session->memoryInfo();
        require(info.sourceIndexValue("json_bytes").as_uint64() == 12 * artifact.byteLength(), 71);
        require(info.sourceIndexValue("blob_bytes").as_uint64() == 0 &&
            info.sourceIndexValue("artifacts").sourceArrayValue().size() == 12, 72);
        auto owned = artifact.value().json;
        require(owned.sourceIndexValue("precision").as_double() ==
            processor->payload.sourceIndexValue("precision").as_double(), 74);
        require(owned.sourceIndexValue("face") == processor->payload.sourceIndexValue("face"), 75);
        auto face = owned.sourceIndexValue("face");
        auto row = face.sourceIndexValue(std::size_t{0});
        // Preserve JSON's original index failure before borrowing its real array.
        (void)row.sourceIndexValue(std::size_t{0});
        auto rowValues = row.sourceArrayValue();
        rowValues[0] = Json(-999.0);
        require(artifact.value().json.sourceIndexValue("face").sourceIndexValue(std::size_t{0})
                .sourceIndexValue(std::size_t{0}) ==
            processor->payload.sourceIndexValue("face").sourceIndexValue(std::size_t{0})
                .sourceIndexValue(std::size_t{0}), 77);
        require(!compat::phobos::ngFileExists(session->directory().view()), 78);
    }
    writeLine({args[1].view(), ": GC measurement unavailable (no GC)"});
    if (session) require(session->task(Text::fromStaticLiteral("stage-11")).artifacts.size() == 1, 88);
    else require(old.size() == 12 && old[old.size() - 1].json.sourceIndexValue("face") ==
        processor->payload.sourceIndexValue("face"), 89);
}
} // namespace autorig_json_storage_test
