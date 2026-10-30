#pragma once
#include <shader.hpp>
#include <render/common.hpp>
#include <math/camera.hpp>
namespace nijigenerate::backend::runtime_gl {
void oglDrawTextureAtPart(const nicxlive::core::Texture* texture,
    const std::shared_ptr<nicxlive::core::nodes::Part>& part);
void oglDrawTextureAtPosition(const nicxlive::core::Texture* texture,
    nicxlive::core::math::Vec2 position, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0});
void oglDrawTextureAtRect(const nicxlive::core::Texture* texture,
    nicxlive::core::Rect area, nicxlive::core::Rect uvs = {0, 0, 1, 1}, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0},
    nicxlive::core::Shader* shader = nullptr, nicxlive::core::math::Camera* camera = nullptr);
void oglDrawTextureAtPart(nicxlive::core::Texture* texture,
    const std::shared_ptr<nicxlive::core::nodes::Part>& part);
void oglDrawTextureAtPosition(nicxlive::core::Texture* texture,
    nicxlive::core::math::Vec2 position, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0});
void oglDrawTextureAtRect(nicxlive::core::Texture* texture,
    nicxlive::core::Rect area, nicxlive::core::Rect uvs = {0, 0, 1, 1}, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0},
    nicxlive::core::Shader* shader = nullptr, nicxlive::core::math::Camera* camera = nullptr);
// Original class value parameters retain their real owners before the first callback.
void oglDrawTextureAtPart(std::shared_ptr<nicxlive::core::Texture> texture,
    std::shared_ptr<nicxlive::core::nodes::Part> part);
void oglDrawTextureAtPosition(std::shared_ptr<nicxlive::core::Texture> texture,
    nicxlive::core::math::Vec2 position, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0});
void oglDrawTextureAtRect(std::shared_ptr<nicxlive::core::Texture> texture,
    nicxlive::core::Rect area, nicxlive::core::Rect uvs = {0, 0, 1, 1}, float opacity = 1,
    nicxlive::core::math::Vec3 color = {1, 1, 1}, nicxlive::core::math::Vec3 screenColor = {0, 0, 0},
    std::shared_ptr<nicxlive::core::Shader> shader = nullptr,
    std::shared_ptr<nicxlive::core::math::Camera> camera = nullptr);
}
