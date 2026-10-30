#include "output_internal.hpp"
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/project.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nijigenerate::widgets::output::detail {

void onParameterView(std::uint64_t index, std::shared_ptr<nicxlive::core::param::Parameter> parameter) {
    // The source casts ulong to int before passing the actual size_t formal.
    const auto narrowed = static_cast<std::int32_t>(index);
    auto puppet = project::incActivePuppet();
    panels::parameters::incParameterView<false, false, true>(static_cast<std::size_t>(narrowed),
        std::move(parameter), &parameterGrabStr, false, puppet->parameters);
}

} // namespace nijigenerate::widgets::output::detail
