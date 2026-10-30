#pragma once

#include "meta.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh::meta {

template<class Host, class Base> class AutoMeshClassInfo;

// Both native mixin transport and direct-base hosts use the same method bodies.
template<class Host> class AutoMeshClassInfoBody {
    template<class, class> friend class AutoMeshClassInfo;
    Host& receiver_;

    auto retainHost() {
        if constexpr (requires(Host& host) { host.weak_from_this().lock(); }) {
            return receiver_.weak_from_this().lock();
        } else {
            return std::shared_ptr<void>{};
        }
    }

public:
    using T = Host;
    explicit AutoMeshClassInfoBody(Host& receiver) : receiver_(receiver) {}
    AutoMeshClassInfoBody(const AutoMeshClassInfoBody&) = delete;
    AutoMeshClassInfoBody& operator=(const AutoMeshClassInfoBody&) = delete;
    AutoMeshClassInfoBody(AutoMeshClassInfoBody&&) = delete;
    AutoMeshClassInfoBody& operator=(AutoMeshClassInfoBody&&) = delete;

    compat::RetainedString procId() {
        compat::RetainedString id;
        forEachProcessorAttribute([&](const auto& attribute) {
            id = attribute.id.empty() ? _fallbackId()
                : compat::RetainedString::fromStaticView(attribute.id);
        });
        return id.empty() ? _fallbackId() : id;
    }

    compat::RetainedString displayName() {
        compat::RetainedString name;
        forEachProcessorAttribute([&](const auto& attribute) {
            name = attribute.displayName.empty() ? compat::RetainedString{}
                : compat::RetainedString::fromStaticView(attribute.displayName);
        });
        if (!name.empty()) return name;
        [[maybe_unused]] const auto owner = retainHost();
        return receiver_.procId();
    }

    int order() {
        auto value = std::numeric_limits<std::int32_t>::min();
        forEachProcessorAttribute([&](const auto& attribute) {
            value = attribute.order;
        });
        return value == std::numeric_limits<std::int32_t>::min() ? 0 : value;
    }

private:
    static compat::RetainedString _fallbackId() {
        return compat::RetainedString::fromStaticView(AMSourceAttributes<T>::identifier);
    }

    template<class Function> static void forEachProcessorAttribute(Function&& function) {
        using Attributes = std::remove_cvref_t<decltype(AMSourceAttributes<T>::attributes)>;
        [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            ([&] {
                [[maybe_unused]] auto attribute = std::get<Indices>(AMSourceAttributes<T>::attributes);
                if constexpr (std::is_same_v<std::tuple_element_t<Indices, Attributes>,
                        AMProcessorSourceAttribute>) {
                    function(attribute);
                }
            }(), ...);
        }(std::make_index_sequence<std::tuple_size_v<Attributes>>{});
    }
};

// Host is the actual lexical typeof(this); Base is its preceding native base.
template<class Host, class Base> class AutoMeshClassInfo : public Base {
    AutoMeshClassInfoBody<Host> body() {
        return AutoMeshClassInfoBody<Host>(static_cast<Host&>(*this));
    }

public:
    using T = Host;
    using Base::Base;
    AutoMeshClassInfo() = default;
    AutoMeshClassInfo(const AutoMeshClassInfo&) = delete;
    AutoMeshClassInfo& operator=(const AutoMeshClassInfo&) = delete;
    AutoMeshClassInfo(AutoMeshClassInfo&&) = delete;
    AutoMeshClassInfo& operator=(AutoMeshClassInfo&&) = delete;

    compat::RetainedString procId() override { return body().procId(); }
    compat::RetainedString displayName() override { return body().displayName(); }
    int order() override { return body().order(); }

private:
    static compat::RetainedString _fallbackId() {
        return AutoMeshClassInfoBody<Host>::_fallbackId();
    }
};

} // namespace nijigenerate::viewport::vertex::automesh::meta
