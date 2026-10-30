// Port of the independent OpenGL drawable_buffers.d production implementation.
// Copyright (c) 2020, Inochi2D Project; (c) 2024, nijigenerate Project.
// BSD-2-Clause; full terms and disclaimer are retained in LICENSE.
#include "runtime_gl_drawable_buffers.hpp"
#include "runtime_gl_soa_upload.hpp"
#include <algorithm>
#include <common/resource_key_map.hpp>
#include <functional>
#define GL_CALL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::backend::runtime_gl {
namespace {
// Process-wide GL resources; all access is owned by the active rendering thread.
unsigned vao{}, vertex{}, uv{}, deform{}, indexBuffer{};
bool initialized{};
std::size_t capacity{}, offset{};
std::size_t nextHandle{1};
using IndexData = nicxlive::core::common::SharedArray<std::uint16_t>;
struct IndexRange { std::size_t offset{}, count{}, capacity{}; IndexData data; };
struct IndexHandleHash {
    std::size_t operator()(std::size_t value) const noexcept { return value; }
};
nicxlive::core::common::ResourceKeyMap<std::size_t, IndexRange,
    IndexHandleHash, std::equal_to<std::size_t>> ranges;
void ensure(std::size_t bytes) {
    if (!indexBuffer) {
        GL_CALL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &indexBuffer);
        capacity = offset = 0;
    }
    if (bytes <= capacity) return;
    auto next = capacity ? capacity : 1024;
    while (next < bytes) {
        next *= 2;
    }
    GL_CALL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    GL_CALL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ELEMENT_ARRAY_BUFFER, next, nullptr, GL_DYNAMIC_DRAW);
    capacity = next; offset = 0;
    for (auto capturedRanges = ranges; auto& [key, entry] : capturedRanges) {
        if (entry.data.empty()) continue;
        const auto size = entry.data.size() * sizeof(std::uint16_t);
        entry.offset = offset; entry.count = entry.data.size(); entry.capacity = size;
        GL_CALL("glBufferSubData", PFNGLBUFFERSUBDATAPROC)(GL_ELEMENT_ARRAY_BUFFER, offset, size, entry.data.data());
        offset += size;
    }
}
unsigned get(unsigned& buffer) {
    if (!buffer) GL_CALL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &buffer);
    return buffer;
}
}
void oglInitDrawableBackend() {
    if (initialized) return;
    initialized = true;
    GL_CALL("glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC)(1, &vao);
}
void oglBindDrawableVao() {
    oglInitDrawableBackend(); GL_CALL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
}
void oglCreateDrawableBuffers(std::size_t& handle) { oglInitDrawableBackend(); if (!handle) handle = nextHandle++; }
// Native queue release extension: handles are slices, never individual GL buffer IDs.
void oglDisposeDrawableBuffers(std::size_t handle) { ranges.erase(handle); }
namespace {
template<class Indices>
void uploadIndices(std::size_t handle, const Indices& indices) {
    if (!handle || indices.empty()) return;
    const auto bytes = indices.size() * sizeof(std::uint16_t);
    ensure(bytes + offset);
    const auto found = ranges.find(handle);
    IndexRange range = found == ranges.end() ? IndexRange{} : found->second;
    if (found == ranges.end() || bytes > range.capacity) {
        range.offset = offset;
        range.count = indices.size();
        range.capacity = bytes;
        offset += bytes;
    } else {
        range.count = indices.size();
    }
    // The source duplicates only after buffer growth/re-upload and range effects.
    IndexData duplicate(indices.size(), std::uint16_t{});
    std::copy(indices.begin(), indices.end(), duplicate.begin());
    range.data = duplicate;
    ranges[handle] = range;
    GL_CALL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    GL_CALL("glBufferSubData", PFNGLBUFFERSUBDATAPROC)(GL_ELEMENT_ARRAY_BUFFER,
        range.offset, bytes, indices.data());
}
}
void oglUploadDrawableIndices(std::size_t handle, const std::vector<std::uint16_t>& indices) {
    uploadIndices(handle, indices);
}
void oglUploadDrawableIndices(std::size_t handle, IndexData indices) {
    uploadIndices(handle, indices);
}
void oglUploadSharedVertexBuffer(nicxlive::core::math::Vec2Array data) {
    if (data.size()) glUploadFloatVecArray(get(vertex), data, GL_DYNAMIC_DRAW, "UploadVertices");
}
void oglUploadSharedUvBuffer(nicxlive::core::math::Vec2Array data) {
    if (data.size()) glUploadFloatVecArray(get(uv), data, GL_DYNAMIC_DRAW, "UploadUV");
}
void oglUploadSharedDeformBuffer(nicxlive::core::math::Vec2Array data) {
    if (data.size()) glUploadFloatVecArray(get(deform), data, GL_DYNAMIC_DRAW, "UploadDeform");
}
unsigned oglGetSharedVertexBuffer() { return get(vertex); }
unsigned oglGetSharedUvBuffer() { return get(uv); }
unsigned oglGetSharedDeformBuffer() { return get(deform); }
void oglDrawDrawableElements(std::size_t handle, std::size_t count) {
    if (!handle || !count) return;
    // Source `in` resolves the actual mapped value before the bind callback.
    // Retain that same cell through removal/replacement, rather than rereading a bucket.
    const auto range = ranges.findRetained(handle);
    if (!range || !indexBuffer) return;
    GL_CALL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    const auto drawOffset = range->offset;
    GL_CALL("glDrawElements", decltype(&::glDrawElements))(GL_TRIANGLES, static_cast<int>(count),
        GL_UNSIGNED_SHORT, reinterpret_cast<const void*>(drawOffset));
}
}
#undef GL_CALL
