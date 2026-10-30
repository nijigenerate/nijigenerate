#pragma once
#include <imgui_internal.h>
#include <cstring>
#include <type_traits>
namespace nijigenerate::widgets::detail {
template<class T> T* prepend(ImVector<T>& vector, ImVector<T>& other) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (other.Size==0) return vector.Data ? vector.Data+vector.Size : nullptr;
    const int newSize=vector.Size+other.Size;
    if (newSize>vector.Capacity) vector.reserve(vector._grow_capacity(newSize));
    std::memmove(vector.Data+other.Size,vector.Data,vector.Size*sizeof(T));
    std::memmove(vector.Data,other.Data,other.Size*sizeof(T));
    vector.Size=newSize;
    return vector.Data+vector.Size;
}
// Window-level completed geometry prefix, not a merger of live path/stacks.
// The source is unsplit and callback-free; destination internal channels and
// callbacks remain supported. See the M311 rendering amendment contract.
void prepend(ImDrawList* self, ImDrawList* other);
}
