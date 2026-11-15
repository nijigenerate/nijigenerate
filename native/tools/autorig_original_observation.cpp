#include <nijigenerate/autorig/deterministic/observation.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <texture.hpp>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    namespace a = nijigenerate::autorig;
    namespace o = a::deterministic::observation;
    namespace e = a::deterministic::evidence;
    namespace c = a::deterministic::contracts;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Array = nijigenerate::core::settings::SettingsJsonArray;
    if (argc != 3) { std::cerr << "Usage: autorig_original_observation original-descriptor.json observation.json\n"; return 2; }
    try {
        std::ifstream input(argv[1]);
        const std::string text(std::istreambuf_iterator<char>(input), {});
        auto state = a::json::ngParseAutoRigJson(a::framework::Text(text));
        std::size_t count = 0;
        for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
            const auto path = std::string(material.sourceIndexValue("texture_file").as_string().view());
            nicxlive::core::ShallowTexture texture(path, 4);
            if (texture.width <= 0 || texture.height <= 0 || texture.data.empty())
                throw std::runtime_error("Cannot decode original texture: " + path);
            nijigenerate::compat::Slice<std::uint8_t> pixels;
            pixels.resize(texture.data.size());
            std::copy(texture.data.begin(), texture.data.end(), pixels.begin());
            const auto mesh = material.sourceIndexValue("source_root_mapping");
            material.sourceIndexAssignSlot("texture_size") = Json(Array{Json(texture.width), Json(texture.height)});
            material.sourceIndexAssignSlot("alpha_runs_32") = o::ngRigAlphaRuns(pixels, 32);
            material.sourceIndexAssignSlot("alpha_runs_128") = o::ngRigAlphaRuns(pixels, 128);
            const auto cloud = o::ngRigTextureSupport(pixels, texture.width, texture.height, mesh, 40000);
            material.sourceIndexAssignSlot("cloud") = c::ngRigPointsJson(cloud);
            material.sourceIndexAssignSlot("active") = material.sourceIndexValue("active").as_bool() && !cloud.empty();
            material.sourceIndexAssignSlot("opaque_perimeter_coverage") =
                o::ngRigAlphaPerimeterCoverage(pixels, texture.width, texture.height);
            auto strong = pixels.duplicate();
            for (std::size_t i = 3; i < strong.size(); i += 4) if (strong[i] <= 128) strong[i] = 0;
            material.sourceIndexAssignSlot("draw_order_cloud") = c::ngRigPointsJson(
                o::ngRigTextureSupport(strong, texture.width, texture.height, mesh, 10000));
            if (e::ngRigMaterialFeature(material.sourceIndexValue("name").as_string()).view() == "sclera") {
                const auto main = o::ngRigLargestAlphaComponent(pixels, texture.width, texture.height);
                material.sourceIndexAssignSlot("landmark_cloud") = c::ngRigPointsJson(
                    o::ngRigTextureSupport(main, texture.width, texture.height, mesh, 40000));
            }
            ++count;
            if (count % 20 == 0) std::cout << "Original Parts sampled: " << count << std::endl;
        }
        auto materials = state.sourceIndexValue("materials").sourceArrayValue();
        e::ngRigPropagateClippingVisibility(materials);
        e::ngRigResolveGroupReceivers(materials, state.sourceIndexValue("groups").sourceArrayValue());
        e::ngRigPropagateClippingVisibility(materials);
        state.sourceIndexAssignSlot("materials") = Json(materials);
        std::ofstream output(argv[2]);
        output << nijigenerate::core::settings::ngSettingsJsonString(state);
        if (!output) throw std::runtime_error("Cannot write original observation");
        std::cout << "Original INX observation completed: " << count << " Parts\n";
        return 0;
    } catch (const nijigenerate::compat::Throwable& error) {
        std::cerr << error.msg.view() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
