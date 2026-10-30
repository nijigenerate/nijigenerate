#include "persistence.hpp"
#include "manifest.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::depth::draw::persistence {
compat::RetainedString ngDepthDrawSessionToPersistentJson(std::shared_ptr<DepthDrawSession> session) {
    auto serialized = ngDepthDrawManifestToJson(ngDepthDrawSessionToManifest(session));
    try {
        return compat::RetainedString(std::move(serialized));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

std::shared_ptr<DepthDrawSession> ngDepthDrawSessionFromPersistentJson(compat::RetainedString value) {
    if (value.empty()) return {};
    autorig::json::detail::FirstPassReader reader(std::move(value));
    return ngDepthDrawSessionFromManifest(reader.parseRoot());
}

bool ngSetPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet,
        std::shared_ptr<DepthDrawSession> session) {
    const auto extended = std::dynamic_pointer_cast<ext::ExPuppet>(puppet);
    if (!extended) return false;
    extended->depthDrawSessionManifestJson = session ? ngDepthDrawSessionToPersistentJson(session) :
        compat::RetainedString{};
    return true;
}

std::shared_ptr<DepthDrawSession> ngGetPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    const auto extended = std::dynamic_pointer_cast<ext::ExPuppet>(puppet);
    if (!extended || extended->depthDrawSessionManifestJson.empty()) return {};
    return ngDepthDrawSessionFromPersistentJson(extended->depthDrawSessionManifestJson);
}

bool ngClearPuppetDepthDrawSession(std::shared_ptr<nicxlive::core::Puppet> puppet) {
    const auto extended = std::dynamic_pointer_cast<ext::ExPuppet>(puppet);
    if (!extended) return false;
    extended->depthDrawSessionManifestJson.reset();
    return true;
}
} // namespace nijigenerate::viewport::depth::draw::persistence
