#pragma once
#include "base.hpp"
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/core/animation/player_runtime.hpp>
#include <nijigenerate/io/videoexport.hpp>
#include <animation/animation_map.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <memory>
#include <cstddef>
#include <string>

namespace nijigenerate::windows::videoexport {
class VideoExportWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    compat::RetainedString outFile;
    std::shared_ptr<nicxlive::core::Puppet> cameraPuppetOwner_;
    std::shared_ptr<ext::nodes::ExCamera> selectedCamera;
    compat::Slice<std::shared_ptr<ext::nodes::ExCamera>> cameras;
    nicxlive::core::AnimationMap animations;
    bool transparency = false, postprocessing = false;
    std::shared_ptr<io::videoexport::VideoEncodingContext> vctx;
    io::videoexport::VideoCodec codec;
    compat::RetainedString animToExport;
    bool done = false, finalizing = false;
    std::shared_ptr<core::animation::AnimationPlayer> player;
    core::animation::AnimationPlaybackRef playback;
    int framerate = -1;
    float lengthFactor = 1;
    float frametime = 0;
    int loops = 0;
    void initialize(compat::RetainedString file);
    void exportFrame();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    VideoExportWindow() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<VideoExportWindow> other);
};
bool ngVideoExportWindowObjectEquals(std::shared_ptr<VideoExportWindow> left,
    std::shared_ptr<VideoExportWindow> right);
}
