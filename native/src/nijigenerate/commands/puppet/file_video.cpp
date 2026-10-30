#include "file_runtime.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/io/videoexport.hpp>
#include <nijigenerate/core/animation/player_runtime.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <runtime_state.hpp>
#include <timing.hpp>
#include <common/shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <thread>
#include <chrono>
#include <bit>
#include <cmath>
namespace nijigenerate::commands::puppet::file {
using namespace detail;
using namespace project;
using namespace io::videoexport;
namespace {
void ngBeginHeadlessVideoExport() {
    ngHeadlessVideoClock=0;
    nicxlive::core::inSetTimingFunc(&ngHeadlessVideoNow);
    nicxlive::core::inUpdate();incActivePuppet()->resetDrivers();
}
void ngEndHeadlessVideoExport() {
    nicxlive::core::inSetTimingFunc(&ngRealtimeNow);incActivePuppet()->resetDrivers();
}
int wrap(std::uint32_t v) { return std::bit_cast<std::int32_t>(v); }
}
std::shared_ptr<CommandResult> ExportVideoCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    using namespace nicxlive::core;
    if(file.empty())return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("File"));
    if(!incActivePuppet())return CommandResult::opCall(false,"No active puppet");
    if(!incVideoCanExport())return CommandResult::opCall(false,"FFMPEG was not found, please install FFMPEG to export video");
    String cameraError;
    auto selectedCamera=ngResolveExportCamera(cameraName,cameraError);
    if(!selectedCamera)return CommandResult::opCall(false,cameraError);
    if(asInt(selectedCamera->getViewport().x)%2!=0||asInt(selectedCamera->getViewport().y)%2!=0)
        return CommandResult::opCall(false,"Camera size must be divisible by 2");
    auto animations=incActivePuppet()->getAnimations();
    if(animations.empty())return CommandResult::opCall(false,"No animations are defined for this model");
    auto selectedAnimation=animationName;
    if(animationName.empty()) {
        compat::Slice<String> names;
        for(auto entry=animations.begin();entry!=animations.end();++entry) {
            names.append(entry->first);
        }
        selectedAnimation=names[0];
    }
    if(animations.find(selectedAnimation)==animations.end())
        return CommandResult::opCall(false,format("Animation not found: %s",selectedAnimation));
    auto selectedCodec=codec.empty()?String::fromStaticLiteral("auto"):codec;
    bool codecFound=selectedCodec.view()=="auto";
    for(const auto& cdc:incVideoCodecs()) if(cdc.tag.view()==selectedCodec.view()) { codecFound=true;break; }
    if(!codecFound)return CommandResult::opCall(false,format("Codec not found: %s",selectedCodec));
    auto player=core::animation::AnimationPlayer::create(incActivePuppet());
    auto playback=player->createOrGet(fileAllocation([&] { return std::string(selectedAnimation.view()); }));
    loops=loops<1?1:loops;
    float frametime=playback->animation()->timestep;
    float lengthFactor=1;
    if(framerate>=1&&framerate!=playback->fps()) {
        lengthFactor=framerate/static_cast<float>(playback->fps());
        frametime=1.f/static_cast<float>(framerate);
    }
    int beginLen=asInt(std::ceil(static_cast<float>(playback->loopPointBegin())*lengthFactor));
    int endLen=asInt(std::ceil((static_cast<float>(playback->animation()->length)-static_cast<float>(playback->loopPointEnd()))*lengthFactor));
    int loopLen=asInt(std::ceil((static_cast<float>(playback->loopPointEnd())-static_cast<float>(playback->loopPointBegin()))*lengthFactor));
    int realLength=wrap(static_cast<std::uint32_t>(beginLen)+static_cast<std::uint32_t>(loopLen)*static_cast<std::uint32_t>(loops)+static_cast<std::uint32_t>(endLen));
    VideoExportSettings settings;
    settings.frames=realLength;settings.framerate=framerate<1?playback->fps():framerate;
    settings.codec=selectedCodec;settings.width=selectedCamera->getViewport().x;settings.height=selectedCamera->getViewport().y;
    settings.file=extension(file).empty()?withExtension(file,"mp4"):file;settings.transparency=transparency;
    auto vctx = std::allocate_shared<VideoEncodingContext>(
        nicxlive::core::d_runtime::SourceAllocator<VideoEncodingContext>{}, settings);
    if(!vctx->checkState())return CommandResult::opCall(false,vctx->errors());
    auto camera=selectedCamera->getCamera();auto vp=selectedCamera->getViewport();
    double timestep=frametime/lengthFactor;
    ngBeginHeadlessVideoExport();
    std::shared_ptr<CommandResult> result;
    compat::ngRunWithScopeExit([&] {
        playback->play(loops>0,true);player->prerenderAll();
        for(int frame=0;frame<settings.frames;++frame) {
            int ow=0,oh=0;inGetViewport(ow,oh);auto oldCamera=inGetCameraHandle();
            float r=std::numeric_limits<float>::quiet_NaN(),g=r,b=r,a=r;
            inSetCamera(camera);inSetViewport(asInt(vp.x),asInt(vp.y));
            if(transparency) { inGetClearColor(r,g,b,a);inSetClearColor(0,0,0,0); }
            compat::ngRunWithScopeExit([&] {
                ngHeadlessVideoClock+=timestep;inUpdate();inBeginScene();incActivePuppet()->update();
                player->update(frametime);incActivePuppet()->draw();inEndScene();
                if(postprocessing)inPostProcessScene();
                if(playback->looped()>=loops)playback->stop(false);
                const auto dataLength = inViewportDataLength();
                auto data = fileAllocation([&] {
                    return nicxlive::core::common::SharedArray<std::uint8_t>(dataLength, 0);
                });
                inDumpViewport(data);vctx->encodeFrame(data);
                if(!vctx->checkState())result=CommandResult::opCall(false,vctx->errors());
            },[&] {
                if(transparency)inSetClearColor(r,g,b,a);
                inSetViewport(ow,oh);inSetCamera(oldCamera);
            });
            if(result)return;
        }
        vctx->end();
        while(!vctx->hasTerminated())std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result=CommandResult::opCall(true);
    },[&] { ngEndHeadlessVideoExport(); });
    return result;
}
}
