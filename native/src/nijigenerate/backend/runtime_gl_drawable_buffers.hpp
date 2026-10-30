#pragma once
#include <math/veca.hpp>
#include <common/shared_array.hpp>
#include <cstdint>
#include <cstddef>
#include <vector>
namespace nijigenerate::backend::runtime_gl {
void oglInitDrawableBackend();
void oglBindDrawableVao();
void oglCreateDrawableBuffers(std::size_t& handle);
void oglDisposeDrawableBuffers(std::size_t handle);
void oglUploadDrawableIndices(std::size_t handle, const std::vector<std::uint16_t>& indices);
void oglUploadDrawableIndices(std::size_t handle, nicxlive::core::common::SharedArray<std::uint16_t> indices);
void oglUploadSharedVertexBuffer(nicxlive::core::math::Vec2Array vertices);
void oglUploadSharedUvBuffer(nicxlive::core::math::Vec2Array uvs);
void oglUploadSharedDeformBuffer(nicxlive::core::math::Vec2Array deformation);
unsigned oglGetSharedVertexBuffer();
unsigned oglGetSharedUvBuffer();
unsigned oglGetSharedDeformBuffer();
void oglDrawDrawableElements(std::size_t handle, std::size_t count);
}
