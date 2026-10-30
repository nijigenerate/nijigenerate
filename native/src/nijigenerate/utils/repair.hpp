#pragma once

#include <memory>

namespace nicxlive::core {
class Puppet;
namespace nodes {
class Node;
}
}

namespace nijigenerate::utils::repair {

void ngAttemptRepairPuppet(std::shared_ptr<nicxlive::core::Puppet> puppet);
void ngRegenerateNodeIDs(std::shared_ptr<nicxlive::core::nodes::Node> node);
void ngPremultTextures(std::shared_ptr<nicxlive::core::Puppet> puppet);

} // namespace nijigenerate::utils::repair
