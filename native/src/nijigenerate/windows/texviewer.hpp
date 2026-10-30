#pragma once

#include "base.hpp"
#include <texture.hpp>
#include <memory>

namespace nijigenerate::windows::texviewer {

class TextureViewerWindow : public base::Window {
    template<class T, class... Args>
    friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);

    std::shared_ptr<nicxlive::core::Texture> texture;
    float zoom = 1.0f;

    void initialize(std::shared_ptr<nicxlive::core::Texture> value);

protected:
    void onBeginUpdate() override;
    void onUpdate() override;

public:
    // Use base::ngCreateWindow to run the logical constructor after shared ownership exists.
    TextureViewerWindow() = default;
};

} // namespace nijigenerate::windows::texviewer
