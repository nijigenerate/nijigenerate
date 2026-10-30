// Independent source/nijigenerate/windows/videoexport.d port. BSD-2-Clause.
#include "videoexport.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <runtime_state.hpp>
#include <timing.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <common/shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::windows::videoexport {
namespace {
void prepareVideoExportClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<VideoExportWindow>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<VideoExportWindow>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(VideoExportWindow), name);
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString VideoExportWindow::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareVideoExportClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t VideoExportWindow::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int VideoExportWindow::opCmp(std::shared_ptr<VideoExportWindow>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareVideoExportClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngVideoExportWindowObjectEquals(std::shared_ptr<VideoExportWindow> left,
    std::shared_ptr<VideoExportWindow> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareVideoExportClassInfo);
}
namespace {
thread_local float acc = 0;
thread_local float forcedTimestep = 1;
constexpr char encodeErrorTag[] = "ENCODE_ERROR";
double exportDeltaTime() { return acc; }
void endExportVideo() {
    nicxlive::core::inSetTimingFunc([] { return ImGui::GetTime(); });
    compat::ngRequireObject(project::incActivePuppet())->resetDrivers();
}
void beginExportVideo(float timestep) {
    acc = 0; forcedTimestep = timestep;
    nicxlive::core::inSetTimingFunc(&exportDeltaTime);
    nicxlive::core::inUpdate();
    compat::ngRequireObject(project::incActivePuppet())->resetDrivers();
}
int asInt(long double value) {
    if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
widgets::dialog::DialogText dialogBody(compat::RetainedString body) {
    using widgets::dialog::DialogText;
    const auto owner = body.retentionOwner();
    return owner.use_count() ? DialogText::retained(body.view(), owner) : DialogText::borrowed(body.view());
}
widgets::dialog::DialogText dialogBody(std::string body) {
    try {
        return widgets::dialog::DialogText::owned(std::move(body));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class MakeBody> void errorDialog(const char* tag, MakeBody makeBody) {
    using namespace widgets::dialog;
    const auto title = core::translation::ngTranslateCString("Error");
    auto body = makeBody();
    ngDialog(DialogTag::borrowed(tag), DialogTag::retained(title.data(), title.owner),
        dialogBody(std::move(body)));
}
}
void VideoExportWindow::exportFrame() {
    const auto receiver = weak_from_this().lock();
    using namespace nicxlive::core;
    auto camera = compat::ngRequireObject(selectedCamera)->getCamera();
    const auto viewport = compat::ngRequireObject(selectedCamera)->getViewport();
    int oldWidth = 0, oldHeight = 0; inGetViewport(oldWidth, oldHeight);
    const auto oldCamera = inGetCameraHandle();
    float red = std::numeric_limits<float>::quiet_NaN();
    float green = red, blue = red, alpha = red;
    inSetCamera(camera); inSetViewport(asInt(viewport.x), asInt(viewport.y));
    if (transparency) { inGetClearColor(red, green, blue, alpha); inSetClearColor(0, 0, 0, 0); }
    acc += forcedTimestep;
    inUpdate();
    inBeginScene();
    compat::ngRequireObject(project::incActivePuppet())->update();
    compat::ngRequireObject(player)->update(frametime);
    compat::ngRequireObject(project::incActivePuppet())->draw();
    inEndScene();
    if (postprocessing) inPostProcessScene();
    if (loops > 0) {
        const auto completedLoops = compat::ngRequireObject(playback)->looped();
        if (completedLoops >= loops) compat::ngRequireObject(playback)->stop(false);
    }
    const auto dataLength = inViewportDataLength();
    nicxlive::core::common::SharedArray<std::uint8_t> data;
    try { data = decltype(data)(dataLength, 0); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    inDumpViewport(data);
    compat::ngRequireObject(vctx)->encodeFrame(data);
    // Source restores state only on the successful path.
    if (transparency) inSetClearColor(red, green, blue, alpha);
    inSetViewport(oldWidth, oldHeight); inSetCamera(oldCamera);
}
void VideoExportWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const float x = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const float width = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const float positionX = x + width / 2;
    const float y = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const float height = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    const ImVec2 position(positionX, y + height / 2);
    const ImVec2 size(720, 420);
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(size, ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void VideoExportWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using core::translation::ngTranslateCString;
    using core::translation::ngTranslateRetained;
    using namespace widgets;
    using namespace io::videoexport;
    if (dialog::ngDialogButtonSelected(encodeErrorTag) == dialog::DialogButtons::OK) close();
    if (vctx && !done) {
        if (compat::ngRequireObject(vctx)->progress() >= 1) {
            if (!finalizing) { compat::ngRequireObject(vctx)->end(); finalizing = true; }
            if (compat::ngRequireObject(vctx)->hasTerminated()) { endExportVideo(); done = true; }
        } else {
            exportFrame();
            if (!compat::ngRequireObject(vctx)->checkState()) {
                using namespace core::translation;
                errorDialog(encodeErrorTag, [this] {
                    const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                        "FFMPEG Encoding Error:\n%s"));
                    const auto errors = compat::ngRequireObject(vctx)->errors();
                    const std::array<TypedFormatArgument, 1> arguments{ngFormatArgument(errors)};
                    return ngFormatTyped(pattern.view(), arguments);
                });
                compat::ngRequireObject(vctx)->end(); vctx.reset();
            }
        }
    }
    ImGui::BeginDisabled(vctx != nullptr);
    if (ImGui::BeginChild("ExportContent", ImVec2(0, -28), ImGuiChildFlags_Borders)) {
        ngText(compat::RetainedStringSlice{
            ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Export Settings"))});
        ImGui::Spacing();
        if (ngBeginCategory(ngTranslateCString("Camera").data())) {
            const auto name = compat::ngRequireObject(selectedCamera)->getName();
            const compat::SourceCString cameraName{compat::RetainedStringSlice{compat::RetainedString{name}}};
            compat::detail::ngSourceCStringPostconditions(name.view(), cameraName.data());
            if (ImGui::BeginCombo("###CAMERA", cameraName.data())) {
                auto currentCameras = cameras;
                for (auto& camera : currentCameras) {
                    const auto nameReceiver = camera;
                    if (ImGui::MenuItem(compat::ngRequireObject(nameReceiver)->cName())) selectedCamera = camera;
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
            ngCheckbox(ngTranslateCString("Allow Transparency").data(), &transparency);
            ngCheckbox(ngTranslateCString("Use Post Processing").data(), &postprocessing);
        }
        ngEndCategory();
        if (ngBeginCategory(ngTranslateCString("Output").data())) {
            ImGui::TextUnformatted(ngTranslateCString("Animation").data());
            const auto animationName = animToExport;
            const compat::SourceCString selectedAnimation{compat::RetainedStringSlice{animationName}};
            compat::detail::ngSourceCStringPostconditions(animationName.view(), selectedAnimation.data());
            if (ImGui::BeginCombo("###Animation", selectedAnimation.data())) {
                auto currentAnimations = compat::ngRequireObject(project::incActivePuppet())->getAnimations();
                for (auto entry = currentAnimations.begin(); entry != currentAnimations.end(); ++entry) {
                    const auto retainedName = entry->first;
                    const compat::SourceCString itemName{compat::RetainedStringSlice{retainedName}};
                    compat::detail::ngSourceCStringPostconditions(retainedName.view(), itemName.data());
                    if (ImGui::MenuItem(itemName.data())) animToExport = retainedName;
                }
                ImGui::EndCombo();
            }
            ImGui::DragInt(ngTranslateCString("Loops").data(), &loops, 1, 0, std::numeric_limits<int>::max());
            ngTooltip(compat::RetainedStringSlice{ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "How many times the animation should loop"))});
            ImGui::DragInt(ngTranslateCString("Framerate").data(), &framerate, 1, -1, 240);
            if (framerate < 1) { ImGui::SameLine(0, 4); ImGui::TextUnformatted(ngTranslateCString("[AUTO]").data()); }
            ngTooltip(compat::RetainedStringSlice{ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "Framerate of the video file"))});
            const auto lockLabel = ngTranslateCString("Lock to Animation Framerate");
            ngCheckbox(lockLabel.data(),
                &compat::ngRequireObject(player)->snapToFramerate);
            ImGui::TextUnformatted(ngTranslateCString("Codec").data()); ImGui::Indent();
            const auto selectedCodecName = codec.name;
            const compat::SourceCString codecName{compat::RetainedStringSlice{selectedCodecName}};
            compat::detail::ngSourceCStringPostconditions(selectedCodecName.view(), codecName.data());
            if (ImGui::BeginCombo("###Codec", codecName.data())) {
                const auto currentCodecs = incVideoCodecs();
                for (const auto item : currentCodecs) {
                    const compat::SourceCString itemName{compat::RetainedStringSlice{item.name}};
                    compat::detail::ngSourceCStringPostconditions(item.name.view(), itemName.data());
                    if (ImGui::MenuItem(itemName.data())) codec = item;
                }
                ImGui::EndCombo();
            }
            ImGui::Unindent(); ngEndCategory();
        }
    }
    const bool userAssertOdd = ext::nodes::ngVerifyCameraSizeShowWarning(selectedCamera);
    ImGui::EndChild(); ImGui::EndDisabled();
    if (ImGui::BeginChild("ExportButtons", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::ProgressBar(vctx ? compat::ngRequireObject(vctx)->progress() : 0, ImVec2(-68, 24));
        ImGui::SameLine(0, 4);
        if (!vctx) {
            ImGui::BeginDisabled(userAssertOdd);
            const bool clicked = ngButtonColored(ngTranslateCString("Export").data(), ImVec2(64, 24));
            ImGui::EndDisabled();
            if (clicked) {
                playback = compat::ngRequireObject(player)->createOrGet(animToExport);
                loops = std::clamp(loops, 1, std::numeric_limits<int>::max());
                frametime = compat::ngRequireObject(compat::ngRequireObject(playback)->animation())->timestep;
                lengthFactor = 1;
                if (framerate >= 1) {
                    const auto comparedFramerate = framerate;
                    if (comparedFramerate != compat::ngRequireObject(playback)->fps()) {
                        const auto numerator = framerate;
                        const auto fps = compat::ngRequireObject(playback)->fps();
                        if (!fps) throw compat::ThrowableError(
                            compat::RetainedString("Animation framerate division by zero"),
                            compat::RetainedString(__FILE__), __LINE__);
                        lengthFactor = static_cast<float>(numerator / fps);
                        frametime = static_cast<float>(1.0 / framerate);
                    }
                }
                const auto beginPoint = static_cast<float>(compat::ngRequireObject(playback)->loopPointBegin());
                const int beginLen = asInt(std::ceil(static_cast<long double>(beginPoint * lengthFactor)));
                const auto animationLength = static_cast<float>(
                    compat::ngRequireObject(compat::ngRequireObject(playback)->animation())->length);
                const auto endPoint = static_cast<float>(compat::ngRequireObject(playback)->loopPointEnd());
                const int endLen = asInt(std::ceil(
                    static_cast<long double>((animationLength - endPoint) * lengthFactor)));
                const auto loopEndPoint = static_cast<float>(compat::ngRequireObject(playback)->loopPointEnd());
                const auto loopBeginPoint = static_cast<float>(compat::ngRequireObject(playback)->loopPointBegin());
                const int loopLen = asInt(std::ceil(
                    static_cast<long double>((loopEndPoint - loopBeginPoint) * lengthFactor)));
                const auto lengthBits = static_cast<std::uint32_t>(beginLen) +
                    static_cast<std::uint32_t>(loopLen) * static_cast<std::uint32_t>(loops) + static_cast<std::uint32_t>(endLen);
                VideoExportSettings settings;
                settings.frames = std::bit_cast<std::int32_t>(lengthBits);
                settings.framerate = framerate < 1 ? compat::ngRequireObject(playback)->fps() : framerate;
                settings.codec = codec.tag;
                settings.width = compat::ngRequireObject(selectedCamera)->getViewport().x;
                settings.height = compat::ngRequireObject(selectedCamera)->getViewport().y;
                settings.file = outFile; settings.transparency = transparency; done = false;
                vctx = std::allocate_shared<VideoEncodingContext>(
                    nicxlive::core::d_runtime::SourceAllocator<VideoEncodingContext>{}, settings);
                beginExportVideo(frametime / lengthFactor);
                compat::ngRequireObject(playback)->play(loops > 0, true);
                compat::ngRequireObject(player)->prerenderAll();
            }
        } else if (!done) {
            if (ngButtonColored(ngTranslateCString("Cancel").data(), ImVec2(64, 24))) {
                compat::ngRequireObject(vctx)->end(); endExportVideo(); close();
            }
        } else if (ngButtonColored(ngTranslateCString("Close").data(), ImVec2(64, 24))) close();
    }
    ImGui::EndChild();
}
void VideoExportWindow::initialize(compat::RetainedString file) {
    const auto receiver = weak_from_this().lock();
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Export Video...")));
    if (!io::videoexport::incVideoCanExport()) {
        errorDialog("NO_EXPORT", [] {
            return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "FFMPEG was not found, please install FFMPEG to export video."));
        });
        close(); return;
    }
    outFile = file;
    const auto queryPuppet = project::incActivePuppet();
    const auto traversalPuppet = compat::ngRequireObject(project::incActivePuppet());
    const auto traversalRoot = traversalPuppet->root;
    auto foundCameras = compat::ngRequireObject(queryPuppet)->findNodesType<ext::nodes::ExCamera>(traversalRoot);
    cameras = {};
    if (!foundCameras.empty()) {
        cameraPuppetOwner_ = traversalPuppet;
        cameras = decltype(cameras)::retainedView(foundCameras.retainPointer(0),
            foundCameras.data(), foundCameras.size());
    }
    if (cameras.empty()) {
        errorDialog("NO_CAMERA", [] {
            return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "No cameras to export from in Scene, please add a Camera."));
        });
        close(); return;
    }
    // Copy the actual shared AA descriptor, independently of the puppet's variable.
    animations = compat::ngRequireObject(project::incActivePuppet())->getAnimations();
    if (animations.empty()) {
        errorDialog("NO_ANIMS", [] {
            return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "No animations are defined for this model, please add an animation."));
        });
        close(); return;
    }
    auto& codecs = io::videoexport::incVideoCodecs();
    if (codecs.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, codecs.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/videoexport.d"), 329);
    codec = codecs[0];
    const auto keys = animations.keys();
    if (keys.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, keys.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/videoexport.d"), 330);
    animToExport = keys[0];
    if (cameras.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, cameras.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/videoexport.d"), 331);
    selectedCamera = cameras[0];
    const auto playerPuppet = project::incActivePuppet();
    try {
        player = core::animation::AnimationPlayer::create(playerPuppet);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
