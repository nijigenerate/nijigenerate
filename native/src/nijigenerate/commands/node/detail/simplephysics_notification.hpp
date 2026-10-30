#pragma once

#include <memory>

namespace nicxlive::core::nodes { class SimplePhysicsDriver; }

namespace nijigenerate::commands::node::simplephysics::detail {

void notifySimplePhysics(std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver> node, bool rescan = false);

}
