#include "nijigenerate/widgets/statusbar_internal.hpp"
#include <utility>

namespace nijigenerate::widgets::statusbar::detail {
compat::Slice<ToolTipItem>& items() {
    static thread_local compat::Slice<ToolTipItem> value;
    return value;
}
}

namespace nijigenerate::widgets {
void ngStatusTooltip(compat::RetainedString action, compat::RetainedString key) {
    auto& index = statusbar::detail::itemIndex();
    auto& items = statusbar::detail::items();
    if (index >= items.size()) items.resize(index + 1);
    const auto destination = index++;
    if (destination >= items.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(destination, items.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/statusbar.d"), 28);
    items.data()[destination] = statusbar::detail::ToolTipItem{std::move(action), std::move(key)};
}
}
