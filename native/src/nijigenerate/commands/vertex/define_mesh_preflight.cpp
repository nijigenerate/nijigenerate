#include "define_mesh_preflight.hpp"
#include <nodes/part.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::vertex::define_mesh::detail {
namespace {
template<std::size_t N> compat::RetainedString topologyFailure(const char (&prefix)[N], std::string_view name) {
    try {
        std::string text(prefix, N - 1);
        text.append(name);
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
} // namespace

bool canApplyMeshTopology(nicxlive::core::nodes::Drawable* target, compat::RetainedString& reason) {
    reason.reset();
    if (auto* part = dynamic_cast<nicxlive::core::nodes::Part*>(target)) {
        const auto texture = part->textures[0];
        if (!texture) {
            const auto name = part->getName();
            reason = topologyFailure("Cannot define mesh for Part without texture: ", name.view());
            return false;
        }
        if (texture->width() == 0 || texture->height() == 0) {
            const auto name = part->getName();
            reason = topologyFailure("Cannot define mesh for Part with empty texture: ", name.view());
            return false;
        }
    }
    return true;
}
}
