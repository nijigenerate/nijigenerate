#include "nijigenerate/autorig/patterns/core.hpp"
#include "nijigenerate/autorig/patterns/detection.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <cmath>

using namespace nijigenerate::autorig::patterns;
using O = boost::json::object;
using A = boost::json::array;
void expect(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    expect(failed, "Expected invalid input to be rejected");
}
Json fixture() {
    return boost::json::parse(R"({"nodes":{"uuid":1,"type":"Node","name":"anonymous","children":[
        {"uuid":2,"type":"Part","name":"a","mesh":{"verts":[0,0,2,0,0,2]},"masks":[]},
        {"uuid":3,"type":"Part","name":"b","mesh":{"verts":[0,0,2,0,0,2]},"masks":[]},
        {"uuid":4,"type":"SimplePhysics","name":"c"},
        {"uuid":5,"type":"MeshGroup","name":"d"}],"transform":{}},"param":[]})");
}
Json control() {
    return O{{"label", "test control"}, {"x", A{-1,0,1}}, {"y", A{-1,0,1}},
        {"origin", A{0,0}}, {"direction", A{1,0}}, {"extent", 2}, {"amount", 0.1},
        {"neutral", 0.5}, {"gain_x", 0.1}, {"gain_y", 0.1}, {"property", "transform.t.x"},
        {"weights", A{0,0.5,1}}};
}
Json manual(std::string_view pattern, const Json& model, int target = 2) {
    O roles{{"target", A{target}}};
    for (const auto& role : ngPatternSpec(pattern).roles) {
        if (role == "support") roles[role] = A{1};
        if (role == "surface" || role == "mask" || role == "seam") roles[role] = A{3};
    }
    return ngDetectPattern(pattern, model, O{{"mode", "manual"}, {"matches", A{O{{"roles", roles}}}}});
}
Json withSource() {
    auto model = fixture();
    auto selection = manual("p32", model, 3);
    auto c = control(); c.as_object()["uuid"] = 100; c.as_object()["label"] = "source";
    return ngDeformPattern("p32", model, selection, O{{"controls", A{c}}}).as_object().at("document");
}
int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            for (int i = 1; i < argc; ++i) {
                std::ifstream input(argv[i], std::ios::binary);
                expect(input.good(), "Cannot open INX fixture");
                std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
                const auto model = ngReadPatternInx(bytes);
                const auto detected = ngDetectStructuralPatterns(model.document);
                O counts;
                for (const auto& spec : ngPatternSpecs()) {
                    const auto& selection = detected.as_object().at(spec.id).as_object();
                    counts[spec.id] = O{{"matches",selection.at("matches").as_array().size()},
                        {"candidates",selection.at("candidates").as_array().size()}};
                }
                std::cout << boost::json::serialize(O{{"model",argv[i]},{"patterns",counts}}) << '\n' << std::flush;
            }
            return 0;
        }
        expect(ngPatternSpecs().size() == 35, "Pattern catalog incomplete");
        {
            auto model = fixture();
            A samples;
            samples.push_back(-0.0);
            for (int i = 1; i <= 1000; ++i)
                samples.push_back(std::ldexp(std::sin(double(i)), i % 80 - 40));
            model.as_object()["numeric_roundtrip"] = samples;
            InxDocument source{model, {'T','R','N','S','R','T','S',0}, {1,2,3}};
            auto restored = ngReadPatternInx(ngWritePatternInx(source, model));
            expect(restored.document == model, "INX serialization changed floating-point values");
            expect(boost::json::serialize(restored.document) == boost::json::serialize(model),
                "INX serialization changed fingerprint input");
            expect(restored.payload == source.payload, "INX serialization changed texture bytes");
        }
        for (const auto& spec : ngPatternSpecs()) {
            std::string p = spec.id;
            auto model = (p == "p01" || p == "p04" || p == "p02") ? withSource() : fixture();
            int target = p == "p30" || p == "p31" ? 4 : p == "p33" ? 5 : p == "p02" ? 3 : 2;
            auto c = control();
            if (p == "p31") c.as_object()["property"] = "outputScale.x";
            Json recipe = O{{"controls", A{c}}};
            if (p == "p08" || p == "p09" || p == "p10") recipe = O{};
            if (p == "p11" || p == "p15") recipe = O{{"blend_mode", "ClipToLower"}};
            if (p == "p16") recipe = O{{"links", A{O{{"source",2},{"target",3},
                {"indices", A{0,-1,2}}, {"weight",0.25}}}}};
            if (p == "p33") recipe = O{{"property","dynamic_deformation"},{"value",true}};
            if (p == "p01" || p == "p04") {
                c.as_object()["source_parameter"] = 100; c.as_object()["source_node"] = 3;
                c.as_object()["property"] = "deform";
                c.as_object()["basis"] = A{A{1,0}, A{0,1}};
                c.as_object()["correspondence"] = A{O{{"indices",A{0}},{"weights",A{1}}},
                    O{{"indices",A{1}},{"weights",A{1}}}, O{{"indices",A{2}},{"weights",A{1}}}};
                recipe = O{{"controls",A{c}}};
            }
            if (p == "p02") {
                auto source = model.as_object().at("param").as_array()[0];
                source.as_object()["uuid"] = 101; source.as_object()["name"] = "driver";
                source.as_object()["bindings"] = A{};
                model.as_object().at("param").as_array().push_back(source);
                c.as_object()["source_parameter"] = 101; c.as_object()["destination_parameter"] = 100;
                c.as_object()["input_axis"] = "X"; c.as_object()["output_axis"] = "Y"; c.as_object()["gain"] = 0.5;
                recipe = O{{"controls",A{c}}};
            }
            if (p == "p28") {
                c.as_object()["anchors"] = A{O{{"position",A{0,0}}, {"motion_x",A{1,0}}, {"motion_y",A{0,1}}},
                    O{{"position",A{2,0}}, {"motion_x",A{-1,0}}, {"motion_y",A{0,-1}}}};
                recipe = O{{"controls",A{c}}};
            }
            if (p == "p29") {
                c.as_object()["modes"] = A{O{{"offsets",A{A{0,0},A{1,0},A{0,1}}}, {"gain_x",1}, {"gain_y",0}}};
                recipe = O{{"controls",A{c}}};
            }
            const auto original = model;
            const auto selection = manual(p, model, target);
            const auto result = ngDeformPattern(p, model, selection, recipe);
            expect(original == model, "Input document changed");
            expect(result.as_object().at("document") != model, "Deformation produced no change");
            auto report = ngVerifyPattern(p, model, result.as_object().at("document"), selection, result.as_object().at("receipt"));
            expect(report.as_object().at("passed").as_bool(), "Verification failed");
            std::cout << p << " passed\n";
        }
        auto model = fixture();
        auto unrigged = model; unrigged.as_object()["param"] = nullptr;
        const auto unriggedSelection = manual("p32", unrigged);
        expect(ngDeformPattern("p32", unrigged, unriggedSelection, O{{"controls",A{control()}}})
            .as_object().at("document").as_object().at("param").as_array().size() == 1,
            "Unrigged null parameter list is not supported");
        auto selection = manual("p32", model);
        auto adjustment = ngDeformPattern("p32", model, selection, O{{"controls",A{control()}}});
        const auto& offsets = adjustment.as_object().at("document").as_object().at("param").as_array()[0]
            .as_object().at("bindings").as_array()[0].as_object().at("values").as_array()[2].as_array()[1].as_array();
        expect(offsets[0].as_array()[0].to_number<double>() == 0 && offsets[0].as_array()[1].to_number<double>() == 0 &&
            std::abs(offsets[2].as_array()[0].to_number<double>() - 0.2) < 1e-9,
            "Protected and free vertex displacement is incorrect");
        auto weld = ngDeformPattern("p16", model, manual("p16",model), O{{"links",A{O{{"source",2},
            {"target",3},{"indices",A{2,-1,0}},{"weight",0.25}}}}});
        const auto& children = weld.as_object().at("document").as_object().at("nodes").as_object().at("children").as_array();
        const auto& reverse = children[1].as_object().at("weldedLinks").as_array()[0].as_object().at("indices").as_array();
        expect(reverse[0].to_number<int>() == 2 && reverse[1].to_number<int>() == -1 && reverse[2].to_number<int>() == 0,
            "Reciprocal welding was not generated");
        auto stale = model; stale.as_object().at("nodes").as_object()["name"] = "changed";
        rejects([&] { ngDeformPattern("p32", stale, selection, O{{"controls", A{control()}}}); });
        rejects([&] { manual("p32", model, 999); });
        auto validMatch = selection.as_object().at("matches").as_array().front();
        auto invalidMatch = validMatch;
        invalidMatch.as_object().at("roles").as_object()["target"] = A{999};
        const auto partial = ngDetectPattern("p32", model, O{{"mode","manual"},
            {"matches", A{validMatch, invalidMatch}}}).as_object();
        expect(partial.at("matches").as_array().size() == 1 && partial.at("unresolved").as_array().size() == 1,
            "Invalid manual candidate blocked a valid structural unit");
        auto empty = ngDetectPattern("p32", model, O{{"mode","manual"},{"matches",A{}}});
        rejects([&] { ngDeformPattern("p32", model, empty, O{{"controls", A{control()}}}); });
        auto bad = control(); bad.as_object()["x"] = A{0,0,1};
        rejects([&] { ngDeformPattern("p32", model, selection, O{{"controls",A{bad}}}); });
        rejects([&] { ngDeformPattern("p32", model, selection, O{{"controls",A{control(),bad}}}); });
        expect(model == fixture(), "Failed multi-control operation modified source");
        auto opacity = control(); opacity.as_object()["gain_x"] = 2;
        rejects([&] { ngDeformPattern("p13", model, manual("p13", model), O{{"controls",A{opacity}}}); });
        auto material = control(); material.as_object()["property"] = "tint.r";
        auto tinted = ngDeformPattern("p15", model, manual("p15",model), O{{"controls",A{material}}});
        expect(tinted.as_object().at("document").as_object().at("param").as_array().size() == 1, "Material control missing");
        auto mask = ngDeformPattern("p08", model, manual("p08", model), O{}).as_object().at("document");
        expect(ngDetectPattern("p08", mask, O{}).as_object().at("matches").as_array().size() == 1, "Mask detection failed");
        expect(ngDetectPattern("p09", mask, O{}).as_object().at("matches").as_array().empty(), "Mask mistaken for dodge mask");
        auto rename = mask; rename.as_object().at("nodes").as_object()["name"] = "irrelevant";
        expect(ngDetectPattern("p08", rename, O{}).as_object().at("parts") ==
            ngDetectPattern("p08", mask, O{}).as_object().at("parts"), "Detection depends on names");
        expect(ngDeformPattern("p08", mask, manual("p08", mask), O{}).as_object().at("document") == mask,
            "Repeated mask application duplicates edges");
        auto query = O{{"condition",O{{"type","Part"},{"parent_type","Node"}}}};
        expect(ngDetectPattern("p32", model, query).as_object().at("parts").as_array().size() == 2, "Condition detector failed");
        InxDocument inx{model, {'T','R','N','S','R','T','S',0}, {0,255,13,10,42}};
        auto bytes = ngWritePatternInx(inx, model);
        auto restored = ngReadPatternInx(bytes);
        expect(restored.document == model && restored.payload == inx.payload, "INX round trip failed");
        bytes.resize(13); rejects([&] { ngReadPatternInx(bytes); });
        bool canceled = false;
        try { ngDetectPattern("p08", model, O{}, [] { throw std::runtime_error("cancel"); }); }
        catch (const std::runtime_error&) { canceled = true; }
        expect(canceled, "Cancellation ignored");
        std::cout << "All pattern and invariant tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
