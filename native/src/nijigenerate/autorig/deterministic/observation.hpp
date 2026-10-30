#pragma once
#include "contracts.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/compat/const_slice.hpp>
namespace nijigenerate::autorig::deterministic::observation {
using Json = core::settings::SettingsJsonValue;
using contracts::Point2;
using Context = std::shared_ptr<framework::AutoRigTaskContext>;
Json ngRigEyelashTopology(Json, Point2, Point2, Context context = {});
Json ngRigAlphaContour(Json, Context context = {}, bool nominalPixelFrame = false);
Json ngRigPrepareShoulders(Json, Json, Context context = {});
Json ngRigAlphaRuns(compat::ConstSlice<std::uint8_t>, std::uint8_t);
double ngRigAlphaCoverage(Json, compat::Slice<Point2>,
    compat::RetainedStringSlice support = compat::RetainedString::fromStaticLiteral("alpha_runs_128"),
    compat::Slice<bool>* occupancy = nullptr);
compat::Slice<std::uint8_t> ngRigLargestAlphaComponent(compat::ConstSlice<std::uint8_t>, std::size_t, std::size_t, Context context = {});
double ngRigAlphaPerimeterCoverage(compat::ConstSlice<std::uint8_t>, std::size_t, std::size_t);
compat::Slice<Point2> ngRigTextureSupport(compat::Slice<std::uint8_t>, std::size_t, std::size_t, Json,
    std::size_t maximumSamples = 40000, Context context = {});
}
