#pragma once
#include <nijigenerate/commands/fields.hpp>
#include <utility>

namespace nijigenerate::commands::node::simplephysics {

// Native representation of a source function-alias argument, not a D class.
// Both the actual declaration and its authored identifier survive substitution.
template<auto Function, CommandLiteral Identifier>
struct SimplePhysicsSourceAlias {
    inline static constexpr auto sourceIdentifier = Identifier;
    template<class... Args>
    decltype(auto) operator()(Args&&... args) const {
        return Function(std::forward<Args>(args)...);
    }
};

} // namespace nijigenerate::commands::node::simplephysics
