#pragma once
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <render/profiler.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <optional>
#include <array>
#include <tuple>
#include <compat/static_error.hpp>
#include <type_traits>
#include <string>
#include <string_view>
namespace nijigenerate::backend::runtime_gl {
// Upload the actual float SoA storage without a CPU layout conversion.
template<class Vec>
void glUploadFloatVecArray(unsigned buffer, const Vec& data, unsigned usage, std::string_view label = {}) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (!buffer || !data.size()) return;
    // Retain D rawStorage's precondition at the actual upload boundary.
    if constexpr (requires { data.laneBase_; data.laneStride_; }) {
#ifndef NDEBUG
        if (!(data.laneBase_ == 0 && (data.laneStride_ == data.size() || data.laneStride_ == 0)))
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::AssertError>(
                nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(
                    "rawStorage is only available for owned contiguous buffers"),
                nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 325);
#endif
    }
    if constexpr (requires { data.rawStorage(); }) {
        const auto raw = data.rawStorage();
        static_assert(std::is_same_v<std::remove_cv_t<std::remove_pointer_t<decltype(raw.ptr)>>, float>);
        if (!raw.length) return;
        std::optional<nicxlive::core::render::RenderProfileScope> scope;
        if (!label.empty()) scope.emplace(std::string(label));
        platform::ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")(GL_ARRAY_BUFFER, buffer);
        platform::ngGLBackendProcedure<PFNGLBUFFERDATAPROC>("glBufferData")(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(raw.length * sizeof(float)), raw.ptr, usage);
    } else {
        // Vec3/4 own separate real lanes. Retain their allocation descriptors
        // before profiling or GL dispatch can invoke external code.
        const auto owners = [&] {
            if constexpr (requires { data.w; }) return std::tuple(data.x, data.y, data.z, data.w);
            else return std::tuple(data.x, data.y, data.z);
        }();
        const auto lanes = [&] {
            if constexpr (requires { data.wPtr_; })
                return std::array<const float*, 4>{data.xPtr_ + data.laneBase_,
                    data.yPtr_ + data.laneBase_, data.zPtr_ + data.laneBase_, data.wPtr_ + data.laneBase_};
            else return std::array<const float*, 3>{data.xPtr_ + data.laneBase_,
                data.yPtr_ + data.laneBase_, data.zPtr_ + data.laneBase_};
        }();
        const auto laneBytes = data.size() * sizeof(float);
        std::optional<nicxlive::core::render::RenderProfileScope> scope;
        if (!label.empty()) scope.emplace(std::string(label));
        platform::ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")(GL_ARRAY_BUFFER, buffer);
        platform::ngGLBackendProcedure<PFNGLBUFFERDATAPROC>("glBufferData")(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(laneBytes * lanes.size()), nullptr, usage);
        for (std::size_t lane = 0; lane < lanes.size(); ++lane)
            platform::ngGLBackendProcedure<PFNGLBUFFERSUBDATAPROC>("glBufferSubData")(
                GL_ARRAY_BUFFER, static_cast<GLintptr>(laneBytes * lane),
                static_cast<GLsizeiptr>(laneBytes), lanes[lane]);
        (void)owners;
    }
#else
    (void)buffer; (void)data; (void)usage; (void)label;
#endif
}
}
