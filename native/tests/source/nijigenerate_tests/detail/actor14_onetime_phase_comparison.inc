// Existing regression.d19931-19946; include after actor14_onetime_snapshot.inc in regression::detail.
void testOneTimeDeformerPreservesUpdatePhasesAndDeformation() {
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer;
    namespace format = nijigenerate::core::translation;
    auto runForType = []<class T>() {
        const auto typeName = [] {
            if constexpr (std::is_same_v<T, GridDeformer>)
                return RetainedString::fromStaticLiteral("GridDeformer");
            else return RetainedString::fromStaticLiteral("PathDeformer");
        }();
        const auto arrayText = [](auto values) {
            // std.conv.toStr: appender, default FormatSpec, formatValue of the actual string array.
            std::string output;
            format::TypedFormatWriter writer(output);
            format::FormatSpec spec;
            format::ngFormatValueTo(writer, values, spec);
            return nijigenerate::platform::detail::ngPathAllocation([&] {
                return RetainedString(std::move(output));
            });
        };
        const auto arraysEqual = [](const auto& left, const auto& right) {
            // core.internal.array.equality.__equals recursively compares these char arrays.
            if (left.size() != right.size()) return false;
            for (std::size_t i = 0; i < left.size(); ++i) {
                if (left[i].size() != right[i].size()) return false;
                if (left[i].size() && std::memcmp(left[i].data(), right[i].data(), left[i].size()) != 0)
                    return false;
            }
            return true;
        };
        auto dynamics = nijigenerate::compat::Slice<bool>{false, true};
        for (std::size_t dynamicIndex = 0; dynamicIndex < dynamics.size(); ++dynamicIndex) {
            const bool dynamic = dynamics[dynamicIndex];
            auto postFilterValues = nijigenerate::compat::Slice<bool>{false, true};
            for (std::size_t postIndex = 0; postIndex < postFilterValues.size(); ++postIndex) {
                const bool postFilters = postFilterValues[postIndex];
                auto reference = oneTimeFilterSnapshot<T>(dynamic, postFilters);
                auto actual = oneTimeFilterSnapshot<OneTimeDeformer<T>>(dynamic, postFilters);
                const bool phasesEqual = arraysEqual(std::get<0>(actual), std::get<0>(reference));
                auto dynamicText = dynamic ? RetainedString::fromStaticLiteral("true") :
                    RetainedString::fromStaticLiteral("false");
                auto postText = postFilters ? RetainedString::fromStaticLiteral("true") :
                    RetainedString::fromStaticLiteral("false");
                auto referenceText = arrayText(std::get<0>(reference));
                auto actualText = arrayText(std::get<0>(actual));
                require(phasesEqual, concatenateSourceStrings({RetainedString::fromStaticLiteral(
                    "temporary filters must preserve the original CPU task phases: "), typeName,
                    RetainedString::fromStaticLiteral(" dynamic="), dynamicText,
                    RetainedString::fromStaticLiteral(" postFilters="), postText,
                    RetainedString::fromStaticLiteral(" reference="), referenceText,
                    RetainedString::fromStaticLiteral(" actual="), actualText}));
                require(nearVec2Array(std::get<1>(actual), std::get<1>(reference)),
                    RetainedString::fromStaticLiteral(
                        "temporary filters must preserve the original grid/path deformation results"));
            }
        }
    };
    runForType.template operator()<GridDeformer>();
    runForType.template operator()<PathDeformer>();
}
