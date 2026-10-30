#include "autorig_state_storage.hpp"
#include <nijigenerate/autorig/deterministic/storage.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <exception>
#include <initializer_list>
#include <system_error>
#include <thread>
#include <utility>

namespace autorig_state_storage_test {
namespace {
namespace compat = nijigenerate::compat;
namespace framework = nijigenerate::autorig::framework;
namespace state = nijigenerate::autorig::deterministic::storage;
namespace settings = nijigenerate::core::settings;
namespace format = nijigenerate::core::translation;
using Json = settings::SettingsJsonValue;
using Array = settings::SettingsJsonArray;
using String = compat::RetainedString;
template<std::size_t N> String text(const char (&value)[N]) { return String::fromStaticLiteral(value); }

void require(bool condition, std::size_t line, String message = text("Assertion failure")) {
    if (!condition) throw compat::ngStaticError<compat::AssertError>(message,
        text("tests/autorig_state_storage.d"), line);
}
Json object(std::initializer_list<std::pair<String, Json>> entries) {
    settings::SettingsJsonObject result;
    for (const auto& [name, value] : entries) result[name] = value;
    return Json(result);
}
Json inputDefault() {
    return object({{text("face"), Json(Array{Json(Array{Json(0.), Json(0.)}),
        Json(Array{Json(1.), Json(0.)}), Json(Array{Json(0.), Json(1.)})})},
        {text("body"), Json(Array{Json(Array{Json(0.), Json(1.)}),
        Json(Array{Json(2.), Json(1.)}), Json(Array{Json(0.), Json(3.)})})}});
}
Json cloud(Json value) { return value.sourceIndexValue("materials").sourceIndexValue(std::size_t{0})
    .sourceIndexValue("cloud"); }
Json firstCoordinate(Json value) { return cloud(value).sourceIndexValue(std::size_t{0})
    .sourceIndexValue(std::size_t{0}); }
void replaceFirstCoordinate(Json value) {
    auto point = cloud(value).sourceIndexValue(std::size_t{0});
    (void)point.sourceIndexValue(std::size_t{0});
    auto coordinates = point.sourceArrayValue();
    coordinates[0] = Json(-999.);
}
void requireCloud(Json value, Array materials, std::size_t line) {
    const auto actual = cloud(value);
    const auto expected = materials[0].sourceIndexValue("cloud");
    require(actual == expected, line);
}
bool sameReferences(Json left, Json right) {
    const auto actual = left.sourceIndexValue("artifact_refs");
    const auto expected = right.sourceIndexValue("artifact_refs");
    return actual == expected;
}

class StorageProcessor final : public framework::AutoRigProcessor {
protected:
    void prepareSourceClassInfo() override {
        AutoRigProcessor::prepareSourceClassInfo();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(StorageProcessor),
            text("autorig_state_storage_test.StorageProcessor"));
    }
public:
    Json source;
    std::shared_ptr<state::RigStateStorage> storage;
    StorageProcessor() { prepareSourceClassInfo(); }
    String procId() override { return text("state-storage-test"); }
    String displayName() override { return text("State storage test"); }
    compat::Slice<framework::AutoRigTaskSpec> tasks() override {
        return {{text("observe"), text("Observe"), {}, {},
            {{text("state"), framework::AutoRigValueKind::Json}}, {}}};
    }
    void executeTask(String, std::shared_ptr<framework::AutoRigTaskContext> context) override {
        storage = state::ngRigStateStorage(context);
        context->publishJson(text("state"), storage->snapshot(source, true));
    }
};

// The used Thread.start/join profile retains the closure until join and
// rethrows the worker's unhandled Throwable on the joining thread.
class StorageWorker {
    std::shared_ptr<state::RigStateStorage> storage_;
    compat::Slice<Json> states_;
    Array materials_;
    std::exception_ptr failure_;
    std::jthread thread_;
public:
    StorageWorker(std::shared_ptr<state::RigStateStorage> storage, compat::Slice<Json> states, Array materials)
        : storage_(storage), states_(states), materials_(materials) {}
    void start() {
        try {
            thread_ = std::jthread([this] {
                try {
                    nijigenerate::core::thread_context::ManagedThreadScope attachment;
                    for (auto checkpoint : states_) {
                        auto restored = storage_->restore(checkpoint);
                        requireCloud(restored, materials_, 63);
                    }
                } catch (...) { failure_ = std::current_exception(); }
            });
        } catch (const std::system_error&) { compat::ngOnThreadError(text("Error creating thread")); }
    }
    void join() {
        try { thread_.join(); }
        catch (const std::system_error&) {
            compat::ngThrowThreadException(text("Unable to join thread"), text("core/thread/osthread.d"),
#if defined(_WIN32)
                501);
#else
                512);
#endif
        }
        if (failure_) std::rethrow_exception(failure_);
    }
};
}

void ngMain(compat::Slice<String> args) {
    require(args.size() <= 2, 29, text("Optionally pass a real PSD-derived cloud JSON path"));
    auto input = args.size() == 2
        ? nijigenerate::autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(args[1]))
        : inputDefault();
    Array materials;
    std::uint64_t id = 0;
    auto inputObject = input.sourceObjectRef();
    for (const auto& member : inputObject) {
        auto name = member.name;
        auto points = member.value();
        materials.append(object({{text("uuid"), Json(++id)}, {text("name"), Json(name)},
            {text("cloud"), points}}));
    }
    auto source = object({{text("materials"), Json(materials)}, {text("rootId"), Json(std::uint64_t{1})}});
    auto storage = std::allocate_shared<state::RigStateStorage>(framework::SourceAllocator<state::RigStateStorage>{});
    auto initial = storage->snapshot(source);
    auto baseline = storage->memoryInfo();
    auto originalBytes = compat::ngJsonStringify(source).size();
    compat::Slice<Json> states;
    for (std::int32_t stage = 0; stage < 14; ++stage) {
        auto owned = storage->restore(initial);
        requireCloud(owned, materials, 45);
        const std::array<format::TypedFormatArgument, 1> argument{format::ngFormatArgument(stage)};
        const auto stageText = [&] { return Json(format::ngFormatTyped("%s", argument)); };
        owned.sourceIndexAssign(text("completed_stage"), [&](Json& slot) { slot = stageText(); }, stageText);
        auto checkpoint = storage->snapshot(owned);
        require(sameReferences(checkpoint, initial), 48);
        require(compat::ngJsonStringify(checkpoint).size() < 1024, 49);
        states.append(checkpoint);
    }
    auto info = storage->memoryInfo();
    require(info == baseline, 53, text("Repeated states duplicated source artifacts or numeric support"));
    auto metadata = storage->artifact(initial, text("materials"));
    auto firstMetadata = metadata.sourceIndexValue(std::size_t{0});
    require(firstMetadata.sourceObjectRef().if_contains("cloud") == nullptr, 55);
    auto changed = storage->restore(initial);
    auto original = firstCoordinate(changed);
    replaceFirstCoordinate(changed);
    require(firstCoordinate(storage->restore(initial)) == original, 59);
    auto worker = std::allocate_shared<StorageWorker>(framework::SourceAllocator<StorageWorker>{},
        storage, states, materials);
    worker->start();
    worker->join();
    for (int index = 0; index < 20; ++index) {
        auto refreshed = storage->snapshot(source, true);
        require(sameReferences(refreshed, initial), 69);
        require(storage->memoryInfo() == baseline, 70,
            text("Fresh identical observations must reuse CPU support and metadata"));
    }
    auto changedSource = storage->restore(initial);
    replaceFirstCoordinate(changedSource);
    auto changedCheckpoint = storage->snapshot(changedSource, true);
    require(!sameReferences(changedCheckpoint, initial), 75);
    require(firstCoordinate(storage->restore(changedCheckpoint)).as_double() == -999., 76);
    auto originalGeneration = storage->snapshot(storage->restore(initial));
    require(sameReferences(originalGeneration, initial), 78);
    storage->dispose();
    require(storage->memoryInfo().sourceIndexValue("json_bytes").as_uint64() == 0, 80);
    require(storage->memoryInfo().sourceIndexValue("cpu_bytes").as_uint64() == 0, 81);
    const auto jsonBytes = info.sourceIndexValue("json_bytes").as_uint64();
    const auto cpuBytes = info.sourceIndexValue("cpu_bytes").as_uint64();
    const std::array<format::TypedFormatArgument, 3> counts{
        format::ngFormatArgument(originalBytes), format::ngFormatArgument(jsonBytes), format::ngFormatArgument(cpuBytes)};
    auto& output = nijigenerate::providers::mcp::stdio::standardOutput();
    output.withTextWriter([&](auto& writer) {
        format::ngFormatTypedTo([&](std::string_view bytes) { writer.put(bytes); },
            "Source JSON: %s bytes; retained metadata: %s bytes; CPU arrays: %s bytes; 14 state references; "
            "ownership and worker round trips passed", counts);
        writer.put(static_cast<unsigned char>('\n'));
    });
    auto processor = std::allocate_shared<StorageProcessor>(framework::SourceAllocator<StorageProcessor>{});
    processor->source = source;
    auto manager = std::allocate_shared<framework::AutoRigSessionManager>(
        framework::SourceAllocator<framework::AutoRigSessionManager>{}, text("out/state-storage-no-files"));
    manager->registerProcessor(processor);
    auto session = manager->create(processor->procId());
    session->execute(text("observe"));
    const auto sessionCpuBytes = session->memoryInfo().sourceIndexValue("cpu_bytes").as_uint64();
    const auto baselineCpuBytes = baseline.sourceIndexValue("cpu_bytes").as_uint64();
    require(sessionCpuBytes == baselineCpuBytes, 89);
    manager->remove(session->id());
    require(session->memoryInfo().sourceIndexValue("json_bytes").as_uint64() == 0, 91);
    require(session->memoryInfo().sourceIndexValue("cpu_bytes").as_uint64() == 0, 92);
    require(processor->storage->memoryInfo().sourceIndexValue("cpu_bytes").as_uint64() == 0, 93);
    output.writeln("Session deletion releases storage even when the session and workspace objects remain referenced");
}
} // namespace autorig_state_storage_test
