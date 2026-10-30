#pragma once

#include <nijigenerate/core/selector/tokenizer.hpp>
#include <common/resource_key_map.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace nijigenerate::commands {
class Command;
namespace model::set_deform_binding {
enum class ModelCommand : std::int32_t;
namespace detail {
struct ModelCommandKeyPolicy {
    struct Hash {
        std::size_t operator()(ModelCommand key) const noexcept {
            // druntime object.d Enum -> base; core/internal/hash.d integral hash (BSL-1.0).
            return static_cast<std::size_t>(static_cast<std::int32_t>(key));
        }
    };
    struct Equal {
        bool operator()(ModelCommand query, ModelCommand stored) const noexcept { return query == stored; }
    };
};
}
}
}

namespace nijigenerate::core::selector {
// Keep the existing canonical map template identity used by command reflection.
// Only this original enum/class-handle pair selects the existing Source AA kernel.
template<> class SelectorMap<commands::model::set_deform_binding::ModelCommand,
        std::shared_ptr<commands::Command>,
        std::hash<commands::model::set_deform_binding::ModelCommand>,
        std::equal_to<commands::model::set_deform_binding::ModelCommand>>
    : public nicxlive::core::common::PolicyKeyMap<
        commands::model::set_deform_binding::ModelCommand, std::shared_ptr<commands::Command>,
        commands::model::set_deform_binding::detail::ModelCommandKeyPolicy> {
    using Key = commands::model::set_deform_binding::ModelCommand;
    using Value = std::shared_ptr<commands::Command>;
    using Base = nicxlive::core::common::PolicyKeyMap<Key, Value,
        commands::model::set_deform_binding::detail::ModelCommandKeyPolicy>;
public:
    void reset() noexcept { Base::operator=(Base{}); }
    SelectorMap duplicate() const {
        SelectorMap result;
        const auto source = *this;
        for (const auto& entry : source) result[entry.first] = entry.second;
        return result;
    }
    template<class... Args> auto try_emplace(Key key, Args&&... args) {
        auto found = this->find(key);
        if (found != this->end()) return std::pair{found, false};
        (*this)[key] = Value(std::forward<Args>(args)...);
        return std::pair{this->find(key), true};
    }
};
}
