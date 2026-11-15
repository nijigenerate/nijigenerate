#pragma once
#include "core.hpp"

namespace nijigenerate::autorig::patterns {
// Observed behavior and geometric proposals are deliberately separate. Neither
// node labels nor preassigned semantic/pattern annotations are consulted here.
Json ngDetectStructuralPatterns(const Json& document, Checkpoint checkpoint = {});
}
