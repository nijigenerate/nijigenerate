#pragma once
#include "context.hpp"
#include <SDL3/SDL_metal.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
namespace nijigenerate::backend::metal {
inline id<MTLTexture> native(const Image& image) { return (__bridge id<MTLTexture>)image.owner.get(); }
template<class T> std::shared_ptr<void> retainMetal(T object) {
    if(!object) throw std::runtime_error("Metal allocation failed");
    return {(__bridge_retained void*)object, [](void* p) { CFRelease(p); }};
}
inline void checkCommand(id<MTLCommandBuffer> command) {
    if(command.status==MTLCommandBufferStatusError) {
        const auto message=command.error.localizedDescription.UTF8String;
        throw std::runtime_error(message?message:"Metal command failed");
    }
}
struct Context::Impl {
    SDL_Window* window{};
    SDL_MetalView view{};
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLCommandBuffer> command, last;
    id<CAMetalDrawable> drawable;
    CAMetalLayer* layer;
    MTLRenderPassDescriptor* uiPass;
    id<MTLLibrary> library;
    id<MTLTexture> uiFormatTexture;
    NSMutableDictionary<NSString*,id<MTLComputePipelineState>>* computePipelines=[NSMutableDictionary new];
    const bool profile=[] { auto v=std::getenv("NIJIGENERATE_METAL_PROFILE"); return v && std::strcmp(v,"0"); }();
    unsigned profileFrames=0;
    double gpuSeconds=0;
    bool imgui=false;
    int width=1,height=1;
    ~Impl() { if(view) SDL_Metal_DestroyView(view); }
};
}
