#pragma once
#include "nijigenerate/widgets/category_flags.hpp"
#include <imgui.h>
#include <functional>
#include <type_traits>
#include <utility>
#include <nijigenerate/compat/shared_delegate.hpp>
namespace nijigenerate::widgets {
using IncCategoryFlags = category::IncCategoryFlags;
using CategoryCallback = void(*)(float, float);
void ngWithReadOnlyCategoryContents(compat::SharedDelegate<void()> draw);
// Legacy callables create their owning context at the boundary; actual source
// delegate descriptors copy the same context directly through the primary.
template<class Callable>
    requires (!std::is_same_v<std::remove_cvref_t<Callable>, compat::SharedDelegate<void()>> &&
        std::is_invocable_r_v<void, Callable&>)
void ngWithReadOnlyCategoryContents(Callable draw) {
    ngWithReadOnlyCategoryContents(compat::SharedDelegate<void()>(std::move(draw)));
}
bool ngBeginCategory(const char* title, IncCategoryFlags flags = IncCategoryFlags::None,
                     CategoryCallback callback = nullptr);
bool ngBeginCategory(const char* title, ImVec4 color, IncCategoryFlags flags = IncCategoryFlags::None,
                     CategoryCallback callback = nullptr);
void ngEndCategory();
}
namespace nijigenerate::widgets::category {
using ::nijigenerate::widgets::ngWithReadOnlyCategoryContents;
using ::nijigenerate::widgets::ngBeginCategory;
using ::nijigenerate::widgets::ngEndCategory;
}
