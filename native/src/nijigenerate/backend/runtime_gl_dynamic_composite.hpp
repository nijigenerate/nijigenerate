#pragma once
#include <render/common.hpp>
namespace nijigenerate::backend::runtime_gl {
#if defined(NIJILIVE_RENDER_PROFILER)
void resetCompositeAccum();
std::uint64_t compositeCpuAccumUsec();
std::uint64_t compositeGpuAccumUsec();
#endif
void oglBeginDynamicComposite(nicxlive::core::DynamicCompositePass& pass);
void oglEndDynamicComposite(nicxlive::core::DynamicCompositePass& pass);
void oglBeginDynamicComposite(nicxlive::core::DynamicCompositePass* pass);
void oglEndDynamicComposite(nicxlive::core::DynamicCompositePass* pass);
void oglDestroyDynamicComposite(const std::shared_ptr<nicxlive::core::DynamicCompositeSurface>& surface);
}
