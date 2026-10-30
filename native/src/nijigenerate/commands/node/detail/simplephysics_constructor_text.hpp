#pragma once
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::commands::node::simplephysics::detail {
// Native carrier for the two ordered translation expressions in real ctors.
struct PhysicsCommandText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}
