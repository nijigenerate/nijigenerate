#include "autorig_head_neck.hpp"
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <nijigenerate/autorig/deterministic/neck.hpp>
#include <nijigenerate/autorig/deterministic/anatomy.hpp>
#include <nijigenerate/autorig/deterministic/templates.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <cmath>
#include <cstddef>

namespace autorig_head_neck_test {
namespace {
namespace compat = nijigenerate::compat;
using String = compat::RetainedString;

void require(bool condition, std::size_t line,
    String message = String::fromStaticLiteral("Assertion failure")) {
    if (!condition) throw compat::ngStaticError<compat::AssertError>(message,
        String::fromStaticLiteral("tests/autorig_head_neck.d"), line);
}
}

void ngMain(compat::Slice<String> args) {
    using namespace nijigenerate::autorig::deterministic;
    using contracts::ngRigPoint;
    using contracts::ngRigPoints;
    require(args.size() > 1, 14, String::fromStaticLiteral("Pass real PSD-derived head/neck JSON paths"));
    for (const auto path : args.subslice(1, args.size())) {
        auto input = nijigenerate::autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(path));
        auto face = ngRigPoints(input.sourceIndexValue("face"));
        auto neckPoints = ngRigPoints(input.sourceIndexValue("neck"));
        auto torso = ngRigPoints(input.sourceIndexValue("torso"));
        auto garments = ngRigPoints(input.sourceIndexValue("garments"));
        // Stage the effectful getters in Source argument order.
        auto eyeRight = ngRigPoint(input.sourceIndexValue("eye_right"));
        auto eyeLeft = ngRigPoint(input.sourceIndexValue("eye_left"));
        auto frame = neck::ngRigInferHeadFrame(face, eyeRight, eyeLeft);
        const compat::Slice<String> keys{
            String::fromStaticLiteral("origin"), String::fromStaticLiteral("tangent"),
            String::fromStaticLiteral("normal"), String::fromStaticLiteral("head_top"),
            String::fromStaticLiteral("head_root")};
        for (const auto key : keys) {
            auto actual = ngRigPoint(frame.sourceIndexValue(key.view()));
            auto expected = ngRigPoint(input.sourceIndexValue("frame").sourceIndexValue(key.view()));
            for (std::size_t axis = 0; axis < 2; ++axis)
                require(std::abs(actual[axis] - expected[axis]) < 1e-9, 22);
        }
        auto result = neck::ngRigInferNeckBase(face, neckPoints, torso, garments, frame);
        auto actual = ngRigPoint(result.sourceIndexValue("xy"));
        auto expected = ngRigPoint(input.sourceIndexValue("inference").sourceIndexValue("xy"));
        for (std::size_t axis = 0; axis < 2; ++axis)
            require(std::abs(actual[axis] - expected[axis]) < 1e-8, 26);
        auto root = ngRigPoint(frame.sourceIndexValue("head_root"));
        auto tangent = ngRigPoint(frame.sourceIndexValue("tangent"));
        auto normal = ngRigPoint(frame.sourceIndexValue("normal"));
        require(std::abs((actual[0] - root[0]) * tangent[0] + (actual[1] - root[1]) * tangent[1]) < 1e-9, 29);
        require((actual[0] - root[0]) * normal[0] + (actual[1] - root[1]) * normal[1] < 0, 30);
        // Retain the Source object descriptor and its actual AA cell.
        auto inputObject = input.sourceObjectRef();
        if (auto* evidence = inputObject.if_contains("evidence")) {
            auto sourceEvidence = *evidence;
            auto prior = templates::ngRigHumanoidPrior();
            auto scaffold = anatomy::ngRigSolveScaffold(sourceEvidence, prior);
            auto landmarkValue = input.sourceIndexValue("scaffold").sourceIndexValue("landmarks");
            auto landmarks = landmarkValue.sourceObjectRef();
            for (const auto& member : landmarks) {
                auto role = member.name;
                auto expectedPoint = member.value();
                auto point = ngRigPoint(scaffold.sourceIndexValue("landmarks").sourceIndexValue(role.view()));
                auto reference = ngRigPoint(expectedPoint);
                for (std::size_t axis = 0; axis < 2; ++axis)
                    require(std::abs(point[axis] - reference[axis]) < 1e-7, 35);
            }
        }
        nijigenerate::providers::mcp::stdio::standardOutput().withTextWriter([&](auto& writer) {
            writer.put(path.view());
            writer.put(": Python/D head frame, neck station and shared axis match");
            writer.put(static_cast<unsigned char>('\n'));
        });
    }
}
} // namespace autorig_head_neck_test
