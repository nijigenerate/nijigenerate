#pragma once
#include <render/common.hpp>
namespace nijigenerate::backend::runtime_gl {
void oglInitMaskBackend();
void oglReleaseMaskBackend();
void oglBeginMask(bool useStencil);
void oglEndMask();
void oglBeginMaskContent();
void oglExecuteMaskPacket(const nicxlive::core::nodes::MaskDrawPacket& packet);
void oglExecuteMaskApplyPacket(const nicxlive::core::RenderBackend::MaskApplyPacket& packet);
}
