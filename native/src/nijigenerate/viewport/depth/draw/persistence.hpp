#pragma once

#include "session.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <puppet.hpp>

namespace nijigenerate::viewport::depth::draw::persistence {
compat::RetainedString ngDepthDrawSessionToPersistentJson(std::shared_ptr<DepthDrawSession> session);
std::shared_ptr<DepthDrawSession> ngDepthDrawSessionFromPersistentJson(compat::RetainedString value);
bool ngSetPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet,
    std::shared_ptr<DepthDrawSession> session);
std::shared_ptr<DepthDrawSession> ngGetPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet);
bool ngClearPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet);
} // namespace nijigenerate::viewport::depth::draw::persistence

namespace nijigenerate::viewport::depth::draw {
// The existing public draw import exposes these actual module functions.
using persistence::ngDepthDrawSessionToPersistentJson;
using persistence::ngDepthDrawSessionFromPersistentJson;
using persistence::ngSetPuppetDepthDrawSession;
using persistence::ngGetPuppetDepthDrawSession;
using persistence::ngClearPuppetDepthDrawSession;
} // namespace nijigenerate::viewport::depth::draw
