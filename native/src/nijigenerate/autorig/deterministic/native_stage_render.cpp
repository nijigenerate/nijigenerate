#include "detail/native_stage.hpp"
#include <nijigenerate/commands/viewport/control_commands.hpp>
#include <nijigenerate/commands/puppet/view_simple_commands.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/io/imagefmt/reader.hpp>
#include <runtime_state.hpp>
#include <math/camera.hpp>
#include <param/parameter.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_base64_exception.hpp>
#include <nijigenerate/compat/phobos_base64.hpp>
#include <nijigenerate/compat/assert_error.hpp>
namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
// The source uses the padded char-array overload of Phobos Base64.
compat::Slice<std::uint8_t> decode64(std::string_view source) {
    const auto capacity=(source.size()/4)*3;
    auto result=allocateNativeBytes(capacity);
    if(source.empty())return {};
    if(source.size()%4!=0)throw compat::Base64Exception(
        compat::RetainedString::fromStaticLiteral("Invalid length of encoded data"),1008);
    auto value=[](char c)->int {
        if(c>='A'&&c<='Z')return c-'A';
        if(c>='a'&&c<='z')return c-'a'+26;
        if(c>='0'&&c<='9')return c-'0'+52;
        if(c=='+')return 62;if(c=='/')return 63;if(c=='=')return -1;
        throw compat::Base64Exception(nativeText({"Invalid character: ",std::string_view(&c,1)}),1731);
    };
    std::size_t written=0;
    for(std::size_t i=0;i<source.size();i+=4) {
        const int a=value(source[i]),b=value(source[i+1]);
        result[written++]=static_cast<std::uint8_t>(a*4|(b>>4));
        const int c=value(source[i+2]);if(c==-1)break;
        result[written++]=static_cast<std::uint8_t>((b*16|(c>>2))&255);
        const int d=value(source[i+3]);if(d==-1)break;
        result[written++]=static_cast<std::uint8_t>((c*64|d)&255);
    }
    // The selected Phobos library out-contract is independent of app NDEBUG.
    const auto expected=capacity-(source[source.size()-2]=='='?2:source.back()=='='?1:0);
    if(written!=expected)throw compat::ngStaticError<compat::AssertError>(
        "The length of result is different from the expected length","std/base64.d",1000);
    return result.slice(0,written);
}
core::settings::SettingsString encode64(std::span<const std::uint8_t> bytes) {
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto length=(bytes.size()/3+(bytes.size()%3?1:0))*4;
    auto result=allocateNativeText(length,'\xff');
    std::size_t written=0;
    for(std::size_t i=0;i<bytes.size();i+=3) {
        const unsigned a=bytes[i],b=i+1<bytes.size()?bytes[i+1]:0,c=i+2<bytes.size()?bytes[i+2]:0;
        result[written++]=alphabet[a>>2];result[written++]=alphabet[((a&3)<<4)|(b>>4)];
        result[written++]=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
        result[written++]=i+2<bytes.size()?alphabet[c&63]:'=';
    }
    if(written!=length)throw compat::ngStaticError<compat::AssertError>(
        "The length of result is different from Base64","std/base64.d",230);
    return core::settings::SettingsString(retainNativeText(std::move(result)));
}
}
Json renderFrame(Task task,framework::Text name,Json cameraFrame) {
    Json frame;compat::Slice<std::uint8_t> pixels;
    task->runOnMainThread([&] {
        auto camera=nicxlive::core::inGetCameraHandle();
        const auto position=camera->position,scale=camera->scale;
        const auto rotation=camera->rotation;
        using namespace viewport::base;
        const auto targetPosition=viewportTargetPosition;
        const auto targetZoom=viewportTargetZoom,zoom=viewportZoom;
        auto restore=[&] {camera->position=position;camera->scale=scale;camera->rotation=rotation;
            viewportTargetPosition=targetPosition;viewportTargetZoom=targetZoom;viewportZoom=zoom;};
        try {
            if(cameraFrame.is_object()) {
                auto p=ngRigNumbers(indexNative(cameraFrame,"position")),s=ngRigNumbers(indexNative(cameraFrame,"scale"));
                camera->position={static_cast<float>(readNativeArray(p,0,314)),static_cast<float>(readNativeArray(p,1,314))};
                camera->scale={static_cast<float>(readNativeArray(s,0,314)),static_cast<float>(readNativeArray(s,1,314))};
                camera->rotation=static_cast<float>(ngRigNumber(indexNative(cameraFrame,"rotation")));
            } else commandSource(
                [&] {
                    return createNativeObject<commands::viewport::control::FitViewportToModelCommand>();
                },
                [&] {
                    return editorContext();
                });
            frame=objectEvidence({{stringLiteralEvidence("position"),array({camera->position.x,camera->position.y})},
                {stringLiteralEvidence("scale"),array({camera->scale.x,camera->scale.y})},{stringLiteralEvidence("rotation"),camera->rotation}});
            auto result=std::dynamic_pointer_cast<commands::ExCommandResult<Json>>(
                commandSource(
                    [&] {
                        return createNativeObject<commands::puppet::view::CaptureLiveScreenshotCommand>();
                    },
                    [&] {
                        return editorContext();
                    }));
            enforceNative(bool(result),"Could not capture rig preview",320);
            pixels=decode64(nativeString(indexNative(indexNative(result->result,"content").sourceIndexValue(std::size_t(0)),"data")).view());
        } catch(...) {restore();throw;}
        restore();
    });
    task->previewBlob(name,pixels,"image/png");
    const std::span<const std::uint8_t> bytes(pixels.data(),pixels.size());
    return objectEvidence({{stringLiteralEvidence("data"),encode64(bytes)},{stringLiteralEvidence("camera"),frame},{stringLiteralEvidence("sha256"),bytesDigest(bytes)}});
}
Json renderValidation(Json state,Task task) {
    if(!state.sourceObjectRef().if_contains("neutral_render"))
        return objectEvidence({{stringLiteralEvidence("applicable"),false},{stringLiteralEvidence("reason"),"Optional rendering disabled"}});
    auto baseline=indexNative(state,"neutral_render"),camera=indexNative(baseline,"camera");
    auto neutral=renderFrame(task,"saved-neutral",camera);
    auto sourceBytes=decode64(nativeString(indexNative(baseline,"data")).view());
    enforceNative(bytesDigest({sourceBytes.data(),sourceBytes.size()}).view()==nativeString(indexNative(baseline,"sha256")).view(),"Neutral render ownership mismatch",334);
    auto source=io::imagefmt::readImage({sourceBytes.data(),sourceBytes.size()},4);
    auto savedBytes=decode64(nativeString(indexNative(neutral,"data")).view());
    auto saved=io::imagefmt::readImage({savedBytes.data(),savedBytes.size()},4);
    Json result;
    compat::ngRunWithScopeExit([&] {
        result = [&]() -> Json {
            auto sourcePixels=source.buf8(),savedPixels=saved.buf8();
            enforceNative(static_cast<int>(source.e)==0&&static_cast<int>(saved.e)==0&&source.w==saved.w&&source.h==saved.h&&
                !sourcePixels.empty()&&sourcePixels.size()==savedPixels.size(),"Neutral preview dimensions changed",337);
            double mean=0,maximum=0;
            for(std::size_t i=0;i<sourcePixels.size();++i) {
                const double difference=std::abs(static_cast<double>(sourcePixels[i])-savedPixels[i]);
                mean+=difference;maximum=std::max(maximum,difference);
            }
            mean/=sourcePixels.size();
            JsonArray poses,bodyPoses,controlPoses;
            if(stringGet(state,"kind","humanoid")=="humanoid") {
                auto reset=[&] {task->runOnMainThread([] {
                    auto puppet=project::incActivePuppet();
                    for(auto parameter:puppet->parameters)parameter->value=parameter->defaults;
                    puppet->update();
                });};
                compat::ngRunWithScopeExit([&] {
                    const std::array<double,5> xs{-1,-.5,0,.5,1};
                    const std::array<double,3> ys{-1,0,1};
                    for(std::size_t x=0;x<xs.size();++x)for(std::size_t y=0;y<ys.size();++y) {
                        ngRigCheckpoint(task);
                        task->runOnMainThread([&] {
                            auto puppet=project::incActivePuppet();
                            parameterByName(puppet,"Face::Yaw-Pitch")->value={static_cast<float>(xs[x]),static_cast<float>(ys[y])};
                            puppet->update();
                        });
                        auto pose=renderFrame(task,nativeText({"head-yaw-",nativeNumber(x).view(),"-pitch-",nativeNumber(y).view()}),camera);
                        assignNative(pose,"parameter","Face::Yaw-Pitch");assignNative(pose,"value",array({xs[x],ys[y]}));poses.append(pose);
                    }
                    auto capture=[&](framework::Text name,std::span<const double> vx,std::span<const double> vy,JsonArray& captures) {
                        const auto stem=nativeCaptureStem(name.view());
                        for(std::size_t x=0;x<vx.size();++x)for(std::size_t y=0;y<vy.size();++y) {
                            ngRigCheckpoint(task);
                            task->runOnMainThread([&] {
                                auto puppet=project::incActivePuppet();
                                for(auto parameter:puppet->parameters)parameter->value=parameter->defaults;
                                parameterByName(puppet,name)->value={static_cast<float>(vx[x]),static_cast<float>(vy[y])};
                                puppet->update();
                            });
                            auto pose=renderFrame(task,nativeText({stem.view(),"-x-",nativeNumber(x).view(),"-y-",nativeNumber(y).view()}),camera);
                            assignNative(pose,"parameter",core::settings::SettingsString(name));assignNative(pose,"value",array({vx[x],vy[y]}));captures.append(pose);
                        }
                    };
                    const std::array<double,1> zero{0};
                    capture("Body::Yaw-Pitch",xs,ys,bodyPoses);capture("Body::Roll",xs,zero,bodyPoses);
                    capture("Face::Roll",xs,zero,poses);
                    if(state.sourceObjectRef().if_contains("controls"))
                        for(auto mechanism:indexNative(indexNative(state,"controls"),"mechanisms").sourceArrayValue()) {
                            auto ax=ngRigNumbers(indexNative(mechanism,"axisX")),ay=ngRigNumbers(indexNative(mechanism,"axisY"));
                            capture(nativeString(indexNative(mechanism,"name")),{ax.data(),ax.size()},{ay.data(),ay.size()},controlPoses);
                        }
                },reset);
            }
            return objectEvidence({{stringLiteralEvidence("applicable"),true},{stringLiteralEvidence("neutral"),neutral},{stringLiteralEvidence("neutral_mean_absolute_error"),mean},
                {stringLiteralEvidence("neutral_maximum_error"),maximum},{stringLiteralEvidence("neutral_passed"),mean<=1},{stringLiteralEvidence("head_support_poses"),poses},
                {stringLiteralEvidence("body_support_poses"),bodyPoses},{stringLiteralEvidence("local_control_poses"),controlPoses},{stringLiteralEvidence("visually_reviewed"),false}});
        }();
    }, [&] { source.free(); saved.free(); });
    return result;
}
}

namespace nijigenerate::compat::phobos {
Slice<std::uint8_t> ngDecodeBase64(RetainedStringSlice source) {
    return ::nijigenerate::autorig::deterministic::native_detail::decode64(source.view());
}
} // namespace nijigenerate::compat::phobos
