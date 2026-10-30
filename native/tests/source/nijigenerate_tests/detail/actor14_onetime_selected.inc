// Existing regression.d19948-19972; include after actor14_onetime_snapshot.inc in regression::detail.
void testOneTimeDeformerSelectedVertices() {
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::math::Vec2;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer;
    auto runForType = []<class T>() {
        const auto typeName = [] {
            if constexpr (std::is_same_v<T, GridDeformer>)
                return RetainedString::fromStaticLiteral("GridDeformer");
            else return RetainedString::fromStaticLiteral("PathDeformer");
        }();
        auto all = std::get<1>(oneTimeFilterSnapshot<OneTimeDeformer<T>>(false, false));
        auto selected = std::get<1>(oneTimeFilterSnapshot<OneTimeDeformer<T>>(
            false, false, Slice<std::uint64_t>{1}));
        auto mirrored = std::get<1>(oneTimeFilterSnapshot<OneTimeDeformer<T>>(
            false, false, Slice<std::uint64_t>{1}, Slice<std::uint64_t>{3}));
        auto bothUnselected = oneTimeFilterSnapshot<OneTimeDeformer<T>>(false, false, {}, {}, true);
        auto otherTargetUnselected = oneTimeFilterSnapshot<OneTimeDeformer<T>>(
            false, false, Slice<std::uint64_t>{1}, {}, true);
        const bool bothMatch = nearVec2Array(std::get<1>(bothUnselected), std::get<2>(bothUnselected));
        require(bothMatch, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "one-time filter without vertex selection must deform every target: "), typeName}));
        auto& otherDeformation = std::get<2>(otherTargetUnselected);
        for (std::size_t i = 0; i < otherDeformation.size(); ++i) {
            auto offset = otherDeformation[i];
            const bool unchanged = nearVec2(offset, Vec2{0, 0});
            require(unchanged, concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "one-time filter must leave unselected targets unchanged: "), typeName}));
        }
        for (std::size_t index = 0; index < all.size(); ++index) {
            const bool selectedMatches = nearVec2(selected[index], index == 1 ? Vec2(all[index]) : Vec2{0, 0});
            require(selectedMatches, concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "one-time filter must deform only selected vertices: "), typeName}));
            const bool mirroredMatches = nearVec2(mirrored[index],
                index == 1 || index == 3 ? Vec2(all[index]) : Vec2{0, 0});
            require(mirroredMatches, concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "one-time filter must include mirrored selected vertices: "), typeName}));
        }
    };
    runForType.template operator()<GridDeformer>();
    runForType.template operator()<PathDeformer>();
}
