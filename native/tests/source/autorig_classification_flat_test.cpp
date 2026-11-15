#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/deterministic/program.hpp>
#include <nijigenerate/autorig/deterministic/observation.hpp>
#include <nijigenerate/autorig/deterministic/storage.hpp>
#include <nijigenerate/autorig/deterministic/contracts_json.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cmath>
#include <bit>
#include <iomanip>

int main(int argc, char** argv) {
    namespace a = nijigenerate::autorig;
    try {
        if (argc == 1) {
            for (const auto& [name, expected] : std::initializer_list<std::pair<const char*, const char*>>{
                    {"Breast::Left", "bodice"}, {"Bust Right", "bodice"}, {"胸", "bodice"},
                    {"Back Head", "hair_back"}, {"後頭部", "hair_back"},
                    {"Boots::Right", "foot"}, {"足::L", "foot"},
                    {"胴体", "torso"}, {"Face Base", "face"}, {"顔面", "face"},
                    {"腕上::R", "arm"}, {"脚下::L", "leg"}, {"One-piece dress", "bodice"}}) {
                const auto roles = a::deterministic::evidence::ngRigMaterialRoleCandidates(a::framework::Text(name));
                if (roles.size() != 1 || roles[0].view() != expected)
                    throw std::runtime_error(std::string("Anatomical candidate missing: ") + name);
            }
        }
        auto input = a::json::ngParseAutoRigJson(a::framework::Text(R"({
            "materials":[{"uuid":1,"name":"Body","path":"/Body","ancestors":[],
                "ancestor_ids":[],"active":true,"cloud":[[0,0],[1,0],[0,1]],
                "landmark_cloud":[[2,3],[4,5]],"draw_order_cloud":[],
                "alpha_runs_32":[],"alpha_runs_128":[],"source_order":0}],"groups":[]
        })"));
        if (argc > 1) {
            std::ifstream stream(argv[1]);
            input = a::json::ngParseAutoRigJson(a::framework::Text(
                std::string(std::istreambuf_iterator<char>(stream), {})));
        }
        auto storage = std::make_shared<a::deterministic::storage::RigStateStorage>();
        if (argc == 1) {
            nijigenerate::core::settings::SettingsJsonArray values;
            values.append(nijigenerate::core::settings::SettingsJsonValue(-0.0));
            for (int i = 0; i < 1000; ++i)
                values.append(nijigenerate::core::settings::SettingsJsonValue(
                    double(float(std::sin(double(i)) * std::pow(10.0, i % 25 - 12)))));
            input.sourceIndexAssignSlot("native_head_keys") = values;
        }
        const bool attachmentsOnly = argc > 3 && std::string(argv[3]) == "--attachments-only";
        const bool compiledInput = attachmentsOnly || (argc > 3 && std::string(argv[3]) == "--compile-classified");
        auto restored = compiledInput ? input : storage->restore(storage->snapshot(input, true));
        if (argc == 1 && a::deterministic::contracts::ngRigDigest(input.sourceIndexValue("native_head_keys")).view() !=
                a::deterministic::contracts::ngRigDigest(restored.sourceIndexValue("native_head_keys")).view()) {
            const auto before = input.sourceIndexValue("native_head_keys").as_array();
            const auto after = restored.sourceIndexValue("native_head_keys").as_array();
            for (std::size_t i = 0; i < before.size(); ++i) {
                if (std::bit_cast<std::uint64_t>(before[i].as_double()) != std::bit_cast<std::uint64_t>(after[i].as_double())) {
                    std::cerr << "Changed value " << i << ": " << std::setprecision(18) << before[i].as_double()
                        << " -> " << after[i].as_double() << " encoded="
                        << nijigenerate::core::settings::ngSettingsJsonString(before[i]) << '\n';
                    break;
                }
            }
            throw std::runtime_error("Restored native head keys changed digest");
        }
        const auto cloud = restored.sourceIndexValue("materials").sourceArrayValue()[0].sourceIndexValue("cloud");
        if (argc == 1 && (cloud.sourceArrayValue().size() != 3 || !cloud.sourceArrayValue()[0].is_array()))
            throw std::runtime_error("Restored point cloud lost its point dimensions");
        if (argc == 1) {
            const auto material = restored.sourceIndexValue("materials").sourceArrayValue()[0];
            const auto landmarks = material.sourceIndexValue("landmark_cloud").sourceArrayValue();
            if (landmarks.size() != 2 || landmarks[1].sourceArrayValue()[1].as_double() != 5.0 ||
                    !material.sourceIndexValue("draw_order_cloud").sourceArrayValue().empty())
                throw std::runtime_error("Restored secondary or empty point cloud changed");
        }
        const auto classified = compiledInput ? restored : a::deterministic::evidence::ngRigClassifyMaterials(restored,
            a::json::ngParseAutoRigJson(a::framework::Text("{}")), {});
        const bool classifyOnly = argc > 3 && std::string(argv[3]) == "--classify-only";
        if (argc > 2) {
            std::ofstream(std::string(argv[2]) + ".ear-pairs.json") <<
                nijigenerate::core::settings::ngSettingsJsonString(
                    a::deterministic::observation::ngRigPrepareLayeredEars(classified));
            if (attachmentsOnly) { std::cout << "Real attachment detection passed\n"; return 0; }
            if (!compiledInput) std::ofstream(std::string(argv[2]) + ".materials.json") <<
                nijigenerate::core::settings::ngSettingsJsonString(classified);
            if (!classifyOnly) {
            const auto evidence = a::deterministic::evidence::ngRigDeriveEvidence(classified);
            std::ofstream(std::string(argv[2]) + ".evidence.json") <<
                nijigenerate::core::settings::ngSettingsJsonString(evidence);
            }
        }
        if (argc > 3 && !classifyOnly) {
            const auto evidence = a::deterministic::evidence::ngRigDeriveEvidence(classified);
            const auto program = a::deterministic::program::ngRigCompileProgram(classified, evidence);
            std::ofstream(std::string(argv[2]) + ".program.json") <<
                nijigenerate::core::settings::ngSettingsJsonString(program);
        }
        std::cout << "Flat PSD classification passed\n";
    } catch (const nijigenerate::compat::Throwable& error) {
        std::cerr << error.msg.view() << " @ " << error.file.view() << ':' << error.line << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
