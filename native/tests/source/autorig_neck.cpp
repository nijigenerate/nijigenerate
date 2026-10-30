// Whole-source port of tests/autorig_neck.d. BSD-2-Clause.
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <nijigenerate/autorig/deterministic/neck.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <cstddef>
#include <utility>

namespace {
using namespace nijigenerate;
void sourceAssert(bool condition, const char* message, unsigned line) {
    if (!condition)
        throw compat::ngStaticError<compat::AssertError>(message, "tests/autorig_neck.d", line);
}
}

int main(int count, char** args) {
    using namespace autorig::deterministic;
    using contracts::ngRigPoint;
    using contracts::ngRigPoints;
    sourceAssert(count > 1, "Pass real PSD-derived cloud JSON paths", 12);
    for (int argument = 1; argument < count; ++argument) {
        auto path = compat::RetainedString(args[argument]);
        auto source = autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(path));
        auto face = ngRigPoints(source.sourceIndexValue("face"));
        auto neckPoints = ngRigPoints(source.sourceIndexValue("neck"));
        auto torso = ngRigPoints(source.sourceIndexValue("torso"));
        auto garments = ngRigPoints(source.sourceIndexValue("garments"));
        auto separate = neck::ngRigInferNeckBase(face, neckPoints, torso, garments);
        // The source concatenation creates independent element storage.
        compat::Slice<contracts::Point2> mergedTorso;
        mergedTorso.appendRange(neckPoints);
        mergedTorso.appendRange(torso);
        auto merged = neck::ngRigInferNeckBase(face, {}, mergedTorso, garments);
        auto a = ngRigPoint(separate.sourceIndexValue("xy"));
        auto b = ngRigPoint(merged.sourceIndexValue("xy"));
        sourceAssert(std::abs(a[0] - b[0]) < 1e-9 && std::abs(a[1] - b[1]) < 1e-9,
            "Part grouping changed the inferred neck location", 20);
        core::settings::SettingsJsonObject output;
        output[core::settings::SettingsString("source")] = core::settings::SettingsJsonValue(
            core::settings::SettingsString(path));
        output[core::settings::SettingsString("inference")] = separate;
        auto text = compat::ngJsonStringify(core::settings::SettingsJsonValue(output));
        providers::mcp::stdio::standardOutput().writeln(text.view());
    }
    return 0;
}
