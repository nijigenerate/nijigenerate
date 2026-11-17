#include <nijigenerate/autorig/patterns/core.hpp>
#include <nijigenerate/autorig/patterns/processor.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace f = nijigenerate::autorig::framework;
namespace p = nijigenerate::autorig::patterns;
namespace fs = std::filesystem;
using O = boost::json::object;

static void writeJson(const fs::path& path, const p::Json& value) {
    std::ofstream stream(path);
    stream << boost::json::serialize(value) << '\n';
    if (!stream) throw std::runtime_error("Cannot write report");
}

static p::Json outputJson(const std::shared_ptr<f::AutoRigSession>& session,
        const char* task, const char* port) {
    return p::ngParsePatternJson(nijigenerate::core::settings::ngSettingsJsonString(session->output(task, port).json));
}

static const char* stateName(f::AutoRigTaskState state) {
    switch (state) {
    case f::AutoRigTaskState::Pending: return "Pending";
    case f::AutoRigTaskState::Running: return "Running";
    case f::AutoRigTaskState::Succeeded: return "Succeeded";
    case f::AutoRigTaskState::Failed: return "Failed";
    case f::AutoRigTaskState::Canceled: return "Canceled";
    case f::AutoRigTaskState::Stale: return "Stale";
    case f::AutoRigTaskState::Blocked: return "Blocked";
    case f::AutoRigTaskState::PartiallySucceeded: return "PartiallySucceeded";
    case f::AutoRigTaskState::NotApplicable: return "NotApplicable";
    }
    throw std::runtime_error("Unknown task state");
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5 || (argc == 5 && !std::string_view(argv[3]).starts_with("--pattern="))) {
        std::cerr << "Usage: autorig_patterns_batch INPUT.inx OUTPUT_DIRECTORY [RECIPES.json|--detect-only|--pattern=ID [RECIPES.json]]\n";
        return 2;
    }
    try {
        const fs::path directory(argv[2]);
        fs::create_directories(directory);
        std::ifstream source(argv[1], std::ios::binary);
        if (!source) throw std::runtime_error("Cannot open INX input");
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(source), {}};
        const auto initial = p::ngReadPatternInx(bytes);
        p::Json recipes = O{};
        const bool detectOnly = argc == 4 && std::string_view(argv[3]) == "--detect-only";
        const auto option = argc >= 4 ? std::string_view(argv[3]) : std::string_view{};
        const auto selectedPattern = option.starts_with("--pattern=") ? option.substr(10) : std::string_view{};
        if (option.starts_with("--pattern=")) (void)p::ngPatternSpec(selectedPattern);
        if (argc == 5 || (argc == 4 && !detectOnly && selectedPattern.empty())) {
            std::ifstream input(argv[argc-1]);
            if (!input) throw std::runtime_error("Cannot open recipes");
            recipes = p::ngParsePatternJson(std::string(std::istreambuf_iterator<char>(input), {}));
        }
        auto manager = std::make_shared<f::AutoRigSessionManager>(f::Text(directory.string()));
        boost::json::array reports;
        bool changed = false;
        for (const auto& spec : p::ngPatternSpecs()) {
            if (!selectedPattern.empty() && spec.id != selectedPattern) continue;
            auto processor = std::make_shared<p::StructuralPatternProcessor>(spec.id);
            manager->registerProcessor(processor);
            auto session = manager->create(processor->procId());
            session->setInput("detect", "model", f::AutoRigValue::blob(
                nijigenerate::compat::Slice<std::uint8_t>(bytes.begin(), bytes.end())));
            const auto supplied = recipes.as_object().if_contains(spec.id);
            const auto selection = supplied && supplied->as_object().contains("selection") ?
                supplied->as_object().at("selection") : p::Json(O{});
            if (!selection.as_object().empty())
                session->setInput("detect", "selection", f::AutoRigValue::jsonValue(
                    nijigenerate::autorig::json::ngParseAutoRigJson(f::Text(boost::json::serialize(selection)))));
            const auto recipe = supplied && supplied->as_object().contains("recipe") ? supplied->as_object().at("recipe") :
                supplied && !supplied->as_object().contains("selection") ? *supplied : p::Json(O{});
            session->setInput("deform", "recipe", f::AutoRigValue::jsonValue(
                nijigenerate::autorig::json::ngParseAutoRigJson(f::Text(boost::json::serialize(recipe)))));
            O record{{"pattern", spec.id}, {"processor", "structural-" + std::string(spec.id)},
                {"requested_task", detectOnly ? "detect" : "verify"},
                {"recipe_source", supplied ? "supplied" : "processor_defaults"}};
            try {
                // Every processor owns its applicability checks. A failed base rig,
                // an empty match set, or an absent authored recipe is not a batch gate.
                session->execute(detectOnly ? "detect" : "verify");
                const auto requested = session->task(detectOnly ? "detect" : "verify", false);
                if (requested.state != f::AutoRigTaskState::Succeeded) {
                    record["state"] = "failed";
                    record["error"] = std::string(requested.message.view());
                } else if (detectOnly) {
                    record["state"] = "detected_only";
                } else {
                    const auto report = outputJson(session, "verify", "report");
                    writeJson(directory/(std::string(spec.id)+"-verification.json"), report);
                    auto after = session->output("verify", "model").readBlob();
                    std::vector<std::uint8_t> next(after.begin(), after.end());
                    const auto parsed = p::ngReadPatternInx(next);
                    if (parsed.payload != initial.payload) throw std::runtime_error("Texture payload changed");
                    const bool modified = parsed.document != p::ngReadPatternInx(bytes).document;
                    changed |= modified;
                    bytes = std::move(next);
                    record["state"] = modified ? "applied" : "verified_unchanged";
                }
            } catch (const nijigenerate::compat::ThrowableException& error) {
                record["state"] = "failed";
                record["error"] = std::string(error.msg.view());
            }
            O tasks;
            for (const auto* task : {"detect", "deform", "verify"}) {
                const auto snapshot = session->task(task, false);
                const bool blocked = !detectOnly && snapshot.state == f::AutoRigTaskState::Pending;
                const auto state = blocked ? "Blocked" : stateName(snapshot.state);
                record[task] = state;
                tasks[task] = O{{"state", state}, {"attempts", snapshot.attempt},
                    {"message", std::string(snapshot.message.view())}};
                if (snapshot.state == f::AutoRigTaskState::Failed) record["failed_task"] = task;
            }
            record["tasks"] = tasks;
            if (session->hasOutput("detect", "targets")) {
                const auto targets = outputJson(session, "detect", "targets");
                writeJson(directory / (std::string(spec.id)+"-targets.json"), targets);
                record["matches"] = targets.as_object().at("matches").as_array().size();
                record["candidates"] = targets.as_object().at("candidates").as_array().size();
                record["detection_status"] = targets.as_object().at("status");
                if (auto* plan = targets.as_object().if_contains("plan")) {
                    writeJson(directory / (std::string(spec.id)+"-plan.json"), *plan);
                    record["plan_status"] = plan->as_object().at("status");
                    record["plan_reason"] = plan->as_object().at("reason");
                }
            }
            if (session->hasOutput("deform", "receipt"))
                writeJson(directory / (std::string(spec.id)+"-receipt.json"), outputJson(session, "deform", "receipt"));
            writeJson(directory / (std::string(spec.id)+"-execution.json"), record);
            reports.push_back(record);
            manager->remove(session->id());
        }
        {
            std::ofstream output(directory/"patterns-result.inx", std::ios::binary);
            output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            if (!output) throw std::runtime_error("Cannot write resulting INX");
        }
        bool complete = true;
        for (const auto& entry : reports) {
            const auto& state = entry.as_object().at("state").as_string();
            complete &= state == "applied" || state == "verified_unchanged";
        }
        writeJson(directory/"report.json", O{{"input",argv[1]}, {"changed",changed}, {"patterns",reports},
            {"output",fs::absolute(directory/"patterns-result.inx").string()},
            {"execution_policy", detectOnly ? "explicit_detection_only" :
                selectedPattern.empty() ? "all_processors_through_verify" : "selected_processor_through_verify"},
            {"visual_reviewed",false}, {"all_applicable_patterns_verified",complete}});
        std::cout << reports.size() << " processor results written to " << directory << '\n';
    } catch (const nijigenerate::compat::ThrowableException& error) {
        std::cerr << error.msg.view() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
