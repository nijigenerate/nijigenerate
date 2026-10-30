#pragma once

#include "nijigenerate/compat/slice.hpp"
#include <cstddef>

namespace nijigenerate::autorig::deterministic::linear {

using RigRow = compat::Slice<double>;
using RigMatrix = compat::Slice<RigRow>;

RigMatrix ngRigSolve(RigMatrix matrix, RigMatrix right);
RigMatrix ngRigZeroMatrix(std::size_t rows, std::size_t columns);
RigMatrix ngRigLeastSquares(RigMatrix matrix, RigMatrix target);

} // namespace nijigenerate::autorig::deterministic::linear
