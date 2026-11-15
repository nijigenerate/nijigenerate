#include <nijigenerate/autorig/deterministic/processor.hpp>
#include <nijigenerate/autorig/deterministic/native.hpp>
#include <nijigenerate/autorig/workflow.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/backend/renderer.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/asyncderivedupdate.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/commands/package.hpp>
#include <nijigenerate/commands/depth/detail/bone_flush.hpp>
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/io/inimport.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/io/imagefmt/writer.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <runtime_state.hpp>
#include <timing.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <boost/json.hpp>

// The production processor runs unchanged; only its application host is replaced.
int main(int argc, char** argv) {
    using namespace nijigenerate;
    namespace f = autorig::framework;
    namespace d = autorig::deterministic;
    namespace fs = std::filesystem;
    const bool captureWithoutPhysics = argc == 9 && std::string_view(argv[8]) == "--no-physics";
    const bool blinkMotion = (argc == 8 || captureWithoutPhysics) &&
        std::string_view(argv[1]) == "--capture-blink-motion";
    const bool capture = blinkMotion ||
        ((argc == 8 || captureWithoutPhysics) && std::string_view(argv[1]) == "--capture-inx");
    const bool customHeadYaw = argc == 5 && std::string_view(argv[3]) == "--head-yaw-degrees";
    if (argc != 3 && !capture && !customHeadYaw) {
        std::cerr << "Usage: autorig_model_batch INPUT.psd OUTPUT_DIRECTORY | --open-only INPUT.inx\n"
                     "       autorig_model_batch INPUT.psd OUTPUT_DIRECTORY --head-yaw-degrees DEGREES\n"
                     "       autorig_model_batch --capture-inx INPUT.inx OUT PARAMETER CENTER_X CENTER_Y ZOOM [--no-physics]\n"
                     "       autorig_model_batch --capture-blink-motion INPUT.inx OUT PARAMETER CENTER_X CENTER_Y ZOOM [--no-physics]\n";
        return 2;
    }
    try {
        const bool openOnly = std::string_view(argv[1]) == "--open-only";
        const fs::path directory = fs::absolute(argv[2]);
        if (!openOnly && !capture) fs::create_directories(directory);
        core::thread_context::ngRegisterMainThread();
        // AutoMesh posts its completion to this in-process queue. No MCP server
        // or network listener is initialized by the standalone host.
        api::mcp::task::ngMcpInitTask();
        nicxlive::core::nodes::inSetUpdateBounds(true);
        core::actionstack::incActionInit();
        backend::ngInitializeRenderer(nullptr);
        nicxlive::core::inInit([] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        });
        nicxlive::core::inSetViewport(1024, 1024);
        ext::incInitExt();
        if (openOnly || capture) {
            const auto puppet = io::project_reader::ngLoadProjectPuppet<ext::ExPuppet>(
                compat::RetainedString(argv[2]));
            puppet->root->build();
            std::cout << "Native INX loaded: " << puppet->textureSlots.size() << " texture slots\n";
            if (capture) {
                namespace rt = nicxlive::core;
                if (captureWithoutPhysics) puppet->enableDrivers = false;
                const fs::path output(argv[3]);
                fs::create_directories(output);
                auto parameter = puppet->findParameterByName(std::string(argv[4]));
                if (!parameter || !parameter->isVec2) throw std::runtime_error("Expected a 2D parameter");
                auto& camera = rt::inGetCamera();
                camera.position = {-std::stof(argv[5]), -std::stof(argv[6])};
                const float zoom = std::stof(argv[7]);
                camera.scale = {zoom, zoom};
                rt::inSetClearColor(0, 0, 0, 0);
                const auto saveFrame = [&](const std::string& name) {
                    puppet->draw();
                    rt::inEndScene();
                    std::vector<std::uint8_t> pixels(rt::inViewportDataLength());
                    rt::inDumpViewport(pixels);
                    rt::inTexUnPremuliply(pixels);
                    auto png = io::imagefmt::writeImageMemory(io::imagefmt::png, 1024, 1024,
                        {pixels.data(), pixels.size()}, 4);
                    if (png.error != io::imagefmt::Error::none) throw std::runtime_error("PNG encoding failed");
                    std::ofstream file(output / (name + ".png"),std::ios::binary);
                    file.write(reinterpret_cast<const char*>(png.buffer.data()),png.length);
                    std::free(png.buffer.detach());
                    if (!file) throw std::runtime_error("Cannot save capture");
                };
                if (blinkMotion) {
                    double time = 0.;
                    rt::inSetTimingFunc([&time] { return time; });
                    puppet->resetDrivers();
                    std::vector<std::shared_ptr<rt::param::Parameter>> springs;
                    for (const auto& driver : puppet->getDrivers()) {
                        const auto spring = std::dynamic_pointer_cast<rt::nodes::SimplePhysicsDriver>(driver);
                        if (spring && spring->getName().view().starts_with("Structural p19/Blink physics/"))
                            if (auto value = spring->getParam()) springs.push_back(std::move(value));
                    }
                    std::ofstream trace(output / "motion.csv");
                    trace << "frame,blink";
                    for (const auto& spring : springs) trace << ',' << spring->getName().view() << " X,Y";
                    trace << '\n';
                    for (int frame=0; frame<120; ++frame) {
                        float blink=0.f;
                        if (frame>=24 && frame<30) blink=static_cast<float>(frame-24)/6.f;
                        if (frame>=30 && frame<36) blink=1.f;
                        if (frame>=36 && frame<44) blink=static_cast<float>(44-frame)/8.f;
                        parameter->value = {blink, 0.f};
                        time = (frame+1)/60.;
                        rt::inUpdate();
                        rt::inBeginScene();
                        puppet->update();
                        trace << frame << ',' << blink;
                        for (const auto& spring : springs) trace << ',' << spring->value.x << ',' << spring->value.y;
                        trace << '\n';
                        if (frame%2 == 0) saveFrame(std::to_string(frame));
                        else rt::inEndScene();
                    }
                    return 0;
                }
                for (int y = -2; y <= 2; ++y) {
                    for (int x = -2; x <= 2; ++x) {
                        parameter->value = {x / 2.0f, y / 2.0f};
                        rt::inBeginScene();
                        puppet->update();
                        if (const auto* id = std::getenv("NJCX_PROBE_DEFORM_NODE")) {
                            auto node = std::dynamic_pointer_cast<rt::nodes::Deformable>(
                                puppet->findNodeById(static_cast<std::uint32_t>(std::stoul(id))));
                            if (!node) throw std::runtime_error("Probe node is not deformable");
                            float maxOffset = 0;
                            const auto& offsets = node->getDeformation();
                            for (std::size_t i = 0; i < offsets.size(); ++i)
                                maxOffset = std::max(maxOffset, std::hypot(offsets.xAt(i), offsets.yAt(i)));
                            std::cout << x << ' ' << y << ' ' << maxOffset << '\n';
                        }
                        saveFrame(std::to_string(x) + "_" + std::to_string(y));
                    }
                }
            }
            return 0;
        }
        // Panel commands require the editor's panel registry, which this host
        // deliberately does not create. Register the ordinary command providers.
        []<std::size_t... I>(std::index_sequence<I...>) {
            const auto initialize = []<class Binding>() {
                using Key = commands::detail::KeyTypeOfCommandMapBinding<Binding>;
                if constexpr (!std::is_same_v<Key, commands::view::panel::PanelKey>) {
                    commands::detail::ngInitCommandMap<Binding>();
                    commands::detail::ngValidateCommandMap<Binding>();
                }
            };
            (initialize.template operator()<std::tuple_element_t<I, commands::AllCommandMaps>>(), ...);
        }(std::make_index_sequence<std::tuple_size_v<commands::AllCommandMaps>>{});
        std::cout << "Importing PSD\n" << std::flush;
        io::inimport::incImport<io::psd::PSD>(core::settings::SettingsString(argv[1]), {}, true);
        nicxlive::core::inUpdate();
        project::incActivePuppet()->update();
        const auto inspect = [&](auto&& self, std::shared_ptr<nicxlive::core::nodes::Node> node) -> void {
            const auto transform = node->transform();
            if (!std::isfinite(transform.translation.x) || !std::isfinite(transform.translation.y)) {
                std::cerr << "Invalid runtime transform: " << node->getName().view()
                    << " local=" << node->localTransform.translation.x << ',' << node->localTransform.translation.y
                    << " offset=" << node->offsetTransform.translation.x << ',' << node->offsetTransform.translation.y << '\n';
            }
            for (auto child : node->children) self(self, child);
        };
        inspect(inspect, project::incActivePuppet()->root);
        auto save = [&](const char* filename) {
            const auto bytes = io::ngWriteProjectPuppetMemory(*project::incActivePuppet());
            std::ofstream output(directory / filename, std::ios::binary);
            output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            if (!output) throw std::runtime_error("Cannot save model");
        };
        save("imported.inx");
        auto sessions = std::make_shared<f::AutoRigSessionManager>(f::Text((directory / "runs").string()));
        std::mutex mutex;
        std::queue<std::shared_ptr<std::packaged_task<void()>>> pending;
        sessions->setEditorDispatcher([&](f::Action action) {
            auto request = std::make_shared<std::packaged_task<void()>>([action] {
                commands::ngRunAutoRigEditorAction(action);
            });
            auto completed = request->get_future();
            { std::lock_guard lock(mutex); pending.push(request); }
            completed.get();
        });
        auto processor = std::make_shared<d::processor::AnimeFrontViewRigProcessor>(
            d::processor::AnimeFrontViewRigProcessor::Projection{}, d::pipeline::RigNativeStage{
                [&directory](auto stage, auto state, auto program, auto model, auto task) {
                    if (stage.view() == "apply-shape-corrections" && std::getenv("NJCX_AUTORIG_REPLAY_DATA")) {
                        // Keep the actual compiler input for repeatable comparison of cheek fields.
                        core::settings::SettingsJsonObject input;
                        input["state"] = state;
                        input["program"] = program;
                        std::ofstream(directory / "cheek-input.json") <<
                            core::settings::ngSettingsJsonString(core::settings::SettingsJsonValue(input));
                    }
                    auto result = d::native::ngRigNativeStage(stage, state, program, model, task);
                    if (stage.view() == "apply-shape-corrections" && std::getenv("NJCX_AUTORIG_REPLAY_DATA"))
                        if (const auto shapes = result.sourceObjectRef().if_contains("rear_hair_silhouettes"))
                            std::ofstream(directory / "rear-hair-silhouettes.json") <<
                                core::settings::ngSettingsJsonString(*shapes);
                    return result;
                }});
        sessions->registerProcessor(processor);
        auto workflows = std::make_shared<autorig::workflow::AutoRigWorkflowManager>(sessions);
        auto run = workflows->create(processor->procId(), "model-to-rig");
        boost::json::object options{{"render", false}};
        if (customHeadYaw) {
            std::size_t parsed = 0;
            const std::string input = argv[4];
            const double degrees = std::stod(input, &parsed);
            if (parsed != input.size() || !(degrees > 0. && degrees < 90.))
                throw std::runtime_error("Head yaw must be between 0 and 90 degrees");
            options["head_yaw_degrees"] = degrees;
        }
        run->setInput("options", f::AutoRigValue::jsonValue(
            autorig::json::ngParseAutoRigJson(f::Text(boost::json::serialize(options)))));
        auto report = [&] {
            boost::json::array steps;
            for (const auto& step : run->orderedSteps()) {
                const auto snapshot = run->session()->task(run->stepTaskId(step.id.retainedString()), false);
                steps.push_back(boost::json::object{{"step", std::string(step.id.view())},
                    {"state", static_cast<int>(snapshot.state)}, {"attempts", snapshot.attempt},
                    {"message", std::string(snapshot.message.view())}});
            }
            const auto snapshot = run->snapshot(false);
            std::ofstream(directory / "report.json") << boost::json::serialize(boost::json::object{
                {"run_id", std::string(run->id().view())}, {"steps", steps},
                {"state", static_cast<int>(snapshot.state)}, {"message", std::string(snapshot.message.view())},
                {"visual_reviewed", false}}) << '\n';
        };
        std::atomic<bool> done = false;
        std::exception_ptr failure;
        std::thread worker([&] {
            try {
                for (const auto& step : run->orderedSteps()) {
                    std::cout << "Stage " << step.id.view() << '\n' << std::flush;
                    run->executeStep(step.id.retainedString());
                    report();
                }
            } catch (...) { failure = std::current_exception(); }
            done = true;
        });
        while (!done) {
            std::shared_ptr<std::packaged_task<void()>> request;
            { std::lock_guard lock(mutex); if (!pending.empty()) { request = pending.front(); pending.pop(); } }
            if (request) (*request)();
            api::mcp::task::ngMcpProcessQueue();
            core::asyncderivedupdate::incAsyncDerivedUpdateUpdate();
            core::tasks::ngTaskUpdate();
            commands::depth::bone::ngFlushDepthBoneDirtyForFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        worker.join();
        save("basic.inx");
        // Retain actual production inputs for numerical replay without importing
        // or applying the rig again merely to inspect a failed finishing stage.
        if (std::getenv("NJCX_AUTORIG_REPLAY_DATA")) for (const auto& [step, port, filename] : {
                std::tuple{"exclusions", "state", "classified-source.json"},
                std::tuple{"layout", "program", "compiled-program.json"},
                std::tuple{"bake", "state", "baked-state.json"},
                std::tuple{"verify", "report", "verification.json"}}) {
            const auto task = run->stepTaskId(f::Text(step));
            if (run->session()->hasOutput(task, f::Text(port)))
                std::ofstream(directory / filename) << core::settings::ngSettingsJsonString(
                    run->session()->output(task, f::Text(port)).json) << '\n';
        }
        report();
        workflows->close(run->id());
        if (failure) std::rethrow_exception(failure);
        std::cout << "Basic AutoRig completed\n";
        return 0;
    } catch (const compat::ThrowableException& error) {
        std::cerr << error.msg.view() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
