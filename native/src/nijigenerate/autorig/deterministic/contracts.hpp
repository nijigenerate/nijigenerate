#pragma once

#include <nijigenerate/compat/fixed_array.hpp>
#include "contracts_file.hpp"
#include <memory>

namespace nijigenerate::autorig::framework { class AutoRigTaskContext; }

namespace nijigenerate::autorig::deterministic::contracts {

using Point2 = compat::FixedArray<double, 2>;
using Point3 = compat::FixedArray<double, 3>;
void ngRigCheckpoint(std::shared_ptr<framework::AutoRigTaskContext> context);

} // namespace nijigenerate::autorig::deterministic::contracts

#include "contracts_json.hpp"
