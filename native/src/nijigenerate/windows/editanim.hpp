#pragma once
#include "base.hpp"
#include <nijigenerate/widgets/inputtext.hpp>
#include <animation/animation.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/translation.hpp>

namespace nijigenerate::windows::editanim {
class EditAnimationWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    bool isNew = false;
    compat::RetainedString originalName;
    compat::RetainedString name;
    nicxlive::core::Animation newAnim;
    std::size_t frameRateOption = 1;
    compat::Slice<core::translation::TranslatedCString> fpsOptions;
    compat::Slice<float> fpsOptionsFR;
    float framerate = 60;
    void apply();
    void initialize();
    void initialize(nicxlive::core::Animation animation, compat::RetainedString name);
protected:
    virtual void setupFPS() final;
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    EditAnimationWindow() = default;
};
}
