#include <nijigenerate/autorig/deterministic/controls.hpp>
#include <nijigenerate/autorig/deterministic/geometry.hpp>
#include <nijigenerate/autorig/deterministic/registered.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/deterministic/program.hpp>
#include <nijigenerate/autorig/deterministic/contracts_json.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    namespace a = nijigenerate::autorig;
    namespace g = a::deterministic::geometry;
    namespace c = a::deterministic::contracts;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Array = nijigenerate::core::settings::SettingsJsonArray;
    if (argc < 3) { std::cerr << "Usage: autorig_controls_replay input.json output.json "
        "[--grid [--interpolation]|--cheek|--registered|--recompile|--exclusions [--compile]|--verify-exclusions]\n"; return 2; }
    try {
        std::ifstream input(argv[1]);
        const auto state = a::json::ngParseAutoRigJson(a::framework::Text(
            std::string(std::istreambuf_iterator<char>(input), {})));
        Json result;
        if (argc > 3 && std::string_view(argv[3]) == "--grid") {
            Array rows;
            const auto rest = c::ngRigPoints(state.sourceIndexValue("rest"));
            const auto columns = static_cast<std::size_t>(c::ngRigUnsigned(state.sourceIndexValue("columns")));
            for (auto key : state.sourceIndexValue("keys").sourceArrayValue()) {
                auto posed = c::ngRigPoints(key.sourceIndexValue("points"));
                const auto maximum = g::ngRigRepairGridOrientation(posed, columns, rest);
                key.sourceIndexAssignSlot("maximum_shift") = maximum;
                key.sourceIndexAssignSlot("validation") = g::ngRigValidateGrid(posed, columns, .02, rest);
                key.sourceIndexAssignSlot("points") = c::ngRigPointsJson(posed);
                rows.append(key);
            }
            if (argc > 4 && std::string_view(argv[4]) == "--interpolation") {
                nijigenerate::compat::Slice<nijigenerate::compat::Slice<c::Point2>> keys;
                std::size_t nx=0,ny=0;
                for(auto row:rows) {
                    keys.append(c::ngRigPoints(row.sourceIndexValue("points")));
                    auto key=c::ngRigNumbers(row.sourceIndexValue("key"));
                    nx=std::max(nx,static_cast<std::size_t>(key[0])+1);
                    ny=std::max(ny,static_cast<std::size_t>(key[1])+1);
                }
                const auto maximum=g::ngRigRepairGridInterpolation(keys,nx,ny,columns,rest);
                for(std::size_t i=0;i<rows.size();++i) {
                    rows[i].sourceIndexAssignSlot("points")=c::ngRigPointsJson(keys[i]);
                    rows[i].sourceIndexAssignSlot("interpolation_maximum_shift")=maximum;
                }
            }
            result = Json(rows);
        } else if (argc > 3 && std::string_view(argv[3]) == "--surface-exclusions") {
            result = state;
            const auto changes = a::deterministic::evidence::ngRigDetectSurfaceExclusions(result);
            result.sourceIndexAssignSlot("surface_exclusions") = changes;
            result.sourceIndexAssignSlot("rig_exclusions").sourceIndexAssignSlot("scoped_parts") = changes;
        } else if (argc > 3 && std::string_view(argv[3]) == "--verify-exclusions") {
            result = a::deterministic::evidence::ngRigVerifyExclusions(
                state.sourceIndexValue("state"), state.sourceIndexValue("program"));
        } else if (argc > 3 && std::string_view(argv[3]) == "--exclusions") {
            result = a::deterministic::evidence::ngRigDetectExclusions(
                state.sourceIndexValue("state"), state.sourceIndexValue("options"));
        } else if (argc > 3 && std::string_view(argv[3]) == "--registered") {
            result = a::deterministic::registered::ngRigRegisterProgram(
                state.sourceIndexValue("program"), state.sourceIndexValue("state"));
        } else if (argc > 3 && std::string_view(argv[3]) == "--recompile") {
            auto observation = state.sourceIndexValue("state");
            result = a::deterministic::program::ngRigCompileProgram(observation,
                observation.sourceIndexValue("evidence"));
        } else if (argc > 3 && std::string_view(argv[3]) == "--cheek-readback") {
            auto model = state.sourceIndexValue("state");
            const auto keys = model.sourceIndexValue("native_head_keys");
            model.sourceIndexAssignSlot("depth_angle_program_sha256") = nijigenerate::core::settings::SettingsString(c::ngRigDigest(keys));
            result = a::deterministic::controls::ngRigCompileCheekCorrections(
                model, state.sourceIndexValue("program"), {});
        } else if (argc > 3 && std::string_view(argv[3]) == "--cheek") {
            result = a::deterministic::controls::ngRigCompileCheekCorrections(
                state.sourceIndexValue("state"), state.sourceIndexValue("program"), {});
        } else result = a::deterministic::controls::ngRigCompileControls(state, {});
        if (argc > 4 && std::string_view(argv[4]) == "--compile"
                && (std::string_view(argv[3]) == "--exclusions" || std::string_view(argv[3]) == "--surface-exclusions")) {
            auto measured = a::deterministic::evidence::ngRigDeriveEvidence(result);
            auto program = a::deterministic::program::ngRigCompileProgram(result, measured);
            const auto validation = a::deterministic::evidence::ngRigVerifyExclusions(result, program);
            result.sourceIndexAssignSlot("exclusion_validation") = validation;
            if (!validation.sourceIndexValue("passed").as_bool()) throw std::runtime_error("Exclusion verification failed");
        }
        std::ofstream output(argv[2]);
        output << nijigenerate::core::settings::ngSettingsJsonString(result);
        if (!output) throw std::runtime_error("Could not write replay output");
        if (argc > 3 && std::string_view(argv[3]) == "--verify-exclusions"
                && !result.sourceIndexValue("passed").as_bool()) return 1;
        std::cout << "Real input replay passed\n";
        return 0;
    } catch (const nijigenerate::compat::Throwable& error) {
        std::cerr << error.msg.view() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
