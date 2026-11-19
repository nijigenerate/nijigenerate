#include <nijigenerate/autorig/deterministic/processor.hpp>
#include <nijigenerate/autorig/deterministic/native.hpp>
#include <nijigenerate/autorig/deterministic/workflow_options.hpp>
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
#include <nodes/drawable.hpp>
#include <nodes/grid_deformer.hpp>
#include <dbg.hpp>
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
#include <sstream>
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
    const bool tagged = argc == 5 && std::string_view(argv[3]) == "--rerun-tag";
    const bool feminineChestDepth = argc == 4 && std::string_view(argv[3]) == "--feminine-chest-depth";
    if (argc != 3 && !capture && !customHeadYaw && !tagged && !feminineChestDepth) {
        std::cerr << "Usage: autorig_model_batch INPUT.psd OUTPUT_DIRECTORY | --open-only INPUT.inx\n"
                     "       autorig_model_batch INPUT.psd OUTPUT_DIRECTORY --head-yaw-degrees DEGREES\n"
                     "       autorig_model_batch INPUT.psd OUTPUT_DIRECTORY --rerun-tag TAG\n"
                     "       autorig_model_batch INPUT.psd OUTPUT_DIRECTORY --feminine-chest-depth\n"
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
                std::vector<std::shared_ptr<rt::nodes::Node>> meshOverlays;
                if (const auto* ids = std::getenv("NJCX_CAPTURE_MESH_IDS")) {
                    std::istringstream list(ids);
                    for (std::string id; std::getline(list, id, ',');) {
                        auto mesh = puppet->findNodeById(static_cast<std::uint32_t>(std::stoul(id)));
                        if (!std::dynamic_pointer_cast<rt::nodes::Drawable>(mesh) &&
                            !std::dynamic_pointer_cast<rt::nodes::GridDeformer>(mesh))
                            throw std::runtime_error("Overlay target must be a drawable or grid deformer");
                        meshOverlays.push_back(std::move(mesh));
                    }
                }
                const auto saveFrame = [&](const std::string& name) {
                    puppet->draw();
                    for (std::size_t i=0; i<meshOverlays.size(); ++i) {
                        const rt::math::Vec4 color = i%2==0 ? rt::math::Vec4{.1f,.9f,1.f,1.f} :
                            rt::math::Vec4{.95f,.2f,.8f,1.f};
                        if (const auto part = std::dynamic_pointer_cast<rt::nodes::Drawable>(meshOverlays[i])) {
                            part->drawMeshLines(color);
                            continue;
                        }
                        const auto grid = std::dynamic_pointer_cast<rt::nodes::GridDeformer>(meshOverlays[i]);
                        const auto vertices = grid->getVertices();
                        const auto offsets = grid->getDeformation();
                        if (vertices.empty()) continue;
                        std::size_t cols = 1;
                        while (cols < vertices.size() && vertices.at(cols).y == vertices.at(0).y) ++cols;
                        if (cols < 2 || vertices.size()%cols != 0) continue;
                        std::vector<rt::math::Vec3> lines;
                        const auto point = [&](std::size_t index) {
                            auto v = vertices.at(index);
                            if (offsets.size() == vertices.size()) v = v + offsets.at(index);
                            return rt::math::Vec3{v.x,v.y,0};
                        };
                        for (std::size_t pos=0; pos<vertices.size(); ++pos) {
                            if (pos%cols+1 < cols) {
                                lines.push_back(point(pos)); lines.push_back(point(pos+1));
                            }
                            if (pos+cols < vertices.size()) {
                                lines.push_back(point(pos)); lines.push_back(point(pos+cols));
                            }
                        }
                        if (!lines.empty()) {
                            rt::dbg::inDbgSetBuffer(std::span<const rt::math::Vec3>(lines));
                            rt::dbg::inDbgDrawLines(color,grid->transform().matrix());
                        }
                    }
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
        if (feminineChestDepth) options[d::feminineChestDepthOption] = true;
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
                boost::json::array units;
                for (const auto& unit : snapshot.units) {
                    if (unit.state == f::AutoRigTaskState::Succeeded) continue;
                    boost::json::array nodes;
                    for (const auto node : unit.nodes) nodes.push_back(node);
                    units.push_back(boost::json::object{{"nodes", std::move(nodes)},
                        {"state", static_cast<int>(unit.state)}, {"message", std::string(unit.message.view())}});
                }
                steps.push_back(boost::json::object{{"step", std::string(step.id.view())},
                    {"state", static_cast<int>(snapshot.state)}, {"attempts", snapshot.attempt},
                    {"message", std::string(snapshot.message.view())}, {"incomplete_units", std::move(units)}});
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
                run->execute();
            } catch (...) { failure = std::current_exception(); }
            done = true;
        });
        const auto pumpEditor = [&] {
            std::shared_ptr<std::packaged_task<void()>> request;
            { std::lock_guard lock(mutex); if (!pending.empty()) { request = pending.front(); pending.pop(); } }
            if (request) (*request)();
            api::mcp::task::ngMcpProcessQueue();
            core::asyncderivedupdate::incAsyncDerivedUpdateUpdate();
            core::tasks::ngTaskUpdate();
            commands::depth::bone::ngFlushDepthBoneDirtyForFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };
        const auto steps = run->orderedSteps();
        std::vector<bool> announced(steps.size());
        auto nextProgress = std::chrono::steady_clock::now();
        while (!done) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextProgress) {
                nextProgress = now + std::chrono::milliseconds(250);
                for (std::size_t i = 0; i < steps.size(); ++i) {
                    if (announced[i]) continue;
                    const auto state = run->session()->task(run->stepTaskId(steps[i].id.retainedString()), false);
                    if (state.state == f::AutoRigTaskState::Pending) continue;
                    if (state.state == f::AutoRigTaskState::Running)
                        std::cout << "Stage " << steps[i].id.view() << '\n' << std::flush;
                    announced[i] = true;
                }
            }
            pumpEditor();
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
        const bool hasRig = run->hasOutput("model");
        if (tagged && hasRig && !failure) {
            const auto model = project::incActivePuppet();
            f::AutoRigTargetSelector selector{{f::Text(argv[4])}, {}, false};
            const auto plan = run->plan(std::move(selector), f::Text(std::to_string(model->root->getUUID())));
            f::AutoRigExecutionReport taggedReport;
            done = false;
            std::thread scopedWorker([&] {
                try { taggedReport = run->execute(plan); }
                catch (...) { failure = std::current_exception(); }
                done = true;
            });
            while (!done) pumpEditor();
            scopedWorker.join();
            if (!failure) {
                save("tagged.inx");
                boost::json::array targets, tasks;
                for (const auto& target : plan.targetScope().targets)
                    targets.push_back(boost::json::object{{"tag", target.tag.view()}, {"node", target.node}});
                for (const auto& task : taggedReport.tasks) {
                    boost::json::array units;
                    for (const auto& unit : task.units) {
                        boost::json::array nodes;
                        for (const auto node : unit.nodes) nodes.push_back(node);
                        units.push_back(boost::json::object{{"nodes", std::move(nodes)},
                            {"state", static_cast<int>(unit.state)}, {"message", unit.message.view()}});
                    }
                    tasks.push_back(boost::json::object{{"task", task.taskId.view()},
                        {"state", static_cast<int>(task.state)}, {"message", task.message.view()},
                        {"units", std::move(units)}});
                }
                std::ofstream(directory / "tagged-report.json") << boost::json::serialize(boost::json::object{
                    {"targets", std::move(targets)}, {"tasks", std::move(tasks)}}) << '\n';
            }
        }
        workflows->close(run->id());
        if (failure) std::rethrow_exception(failure);
        for (const auto& step : run->orderedSteps()) {
            const auto state = run->session()->task(run->stepTaskId(step.id.retainedString()), false);
            if (state.state == f::AutoRigTaskState::Failed || state.state == f::AutoRigTaskState::Blocked)
                std::cout << step.id.view() << ": " << state.message.view() << '\n';
        }
        std::cout << "Basic AutoRig run finished\n";
        return hasRig ? 0 : 1;
    } catch (const compat::ThrowableException& error) {
        std::cerr << error.msg.view() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
