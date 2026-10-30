#pragma once

// Public source imports, preserving the original order and canonical owners.
#include "automesh.hpp"
#include "contours.hpp"
#include "grid.hpp"
#include "skeletonize.hpp"
#include "optimum.hpp"
#include "alpha_provider.hpp"
#include <nijigenerate/compat/type_sequence.hpp>

namespace nijigenerate::viewport::vertex::automesh {

using contours::ContourAutoMeshProcessor;
using grid::GridAutoMeshProcessor;
using skeletonize::Point;
using skeletonize::SkeletonExtractor;
using optimum::OptimumAutoMeshProcessor;
using optimum::stdFilter;
using optimum::stdUniq;
using optimum::mirAny;

using AutoMeshProcessorTypes = compat::TypeSequence<
    optimum::OptimumAutoMeshProcessor,
    contours::ContourAutoMeshProcessor,
    grid::GridAutoMeshProcessor,
    skeletonize::SkeletonExtractor>;

} // namespace nijigenerate::viewport::vertex::automesh
