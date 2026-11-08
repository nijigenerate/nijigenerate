#include "context_internal.hpp"
#include "metal_shaders.hpp"
#include <SDL3/SDL_metal.h>
#include <imgui_impl_metal.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void*);
namespace nijigenerate::backend::metal {
namespace { Context* current{}; void* framePool{}; }
Context& context() {
    if(!current) throw std::logic_error("Metal context is not active");
    return *current;
}
Context::Context(SDL_Window* window) : p(std::make_unique<Impl>()) {
    @autoreleasepool {
        p->window=window;
        p->device=MTLCreateSystemDefaultDevice();
        if(!p->device) throw std::runtime_error("No Metal device is available");
        // Framebuffer fetch and ordered attachment access are Apple GPU features.
        advancedBlendSupported=advancedBlendCoherent=[p->device supportsFamily:MTLGPUFamilyApple4];
        p->queue=[p->device newCommandQueue];
        p->view=SDL_Metal_CreateView(window);
        if(!p->view) throw std::runtime_error(SDL_GetError());
        p->layer=(__bridge CAMetalLayer*)SDL_Metal_GetLayer(p->view);
        p->layer.device=p->device;
        p->layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
        p->layer.framebufferOnly=YES;
        p->layer.opaque=YES;
        p->layer.displaySyncEnabled=YES;
        p->layer.maximumDrawableCount=2;
        NSError* error=nil;
        MTLCompileOptions* options=[MTLCompileOptions new];
        options.languageVersion=MTLLanguageVersion2_3;
        options.fastMathEnabled=NO;
        options.preprocessorMacros=@{@"NG_FRAMEBUFFER_FETCH":@(advancedBlendSupported)};
        p->library=[p->device newLibraryWithSource:[NSString stringWithUTF8String:render_source] options:options error:&error];
        if(!p->library) throw std::runtime_error(error.localizedDescription.UTF8String);
        resize();
        current=this;
    }
}
Context::~Context() {
    idle(); shutdownImGui();
    p->command=nil; p->last=nil; p->drawable=nil; p->layer=nil;
    p.reset(); current=nullptr;
    if(framePool) { objc_autoreleasePoolPop(framePool); framePool=nullptr; }
}
void Context::idle() {
    if(p->last) {
        [p->last waitUntilCompleted]; checkCommand(p->last);
        if(p->profile && p->last.GPUEndTime>=p->last.GPUStartTime) {
            p->gpuSeconds+=p->last.GPUEndTime-p->last.GPUStartTime;
            if(++p->profileFrames==120) {
                std::fprintf(stderr,"[metal-gpu] frame_gpu_ms=%.3f (includes editor UI)\n",p->gpuSeconds*1000/120);
                p->profileFrames=0; p->gpuSeconds=0;
            }
        }
        p->last=nil;
    }
}
void Context::resize() {
    SDL_GetWindowSizeInPixels(p->window,&p->width,&p->height);
    const auto size=CGSizeMake(p->width,p->height);
    if(p->width>0 && p->height>0 && !CGSizeEqualToSize(p->layer.drawableSize,size)) p->layer.drawableSize=size;
}
void Context::setThrottle(int rate) { p->layer.displaySyncEnabled=rate>0; }
void Context::initializeImGui() {
    p->imgui=ImGui_ImplMetal_Init(p->device);
    if(!p->imgui) throw std::runtime_error("ImGui Metal initialization failed");
}
void Context::shutdownImGui() {
    if(p && p->imgui) { idle(); ImGui_ImplMetal_Shutdown(); p->imgui=false; }
}
void Context::newFrame() {
    idle();
    if(framePool) objc_autoreleasePoolPop(framePool);
    framePool=objc_autoreleasePoolPush();
    resize();
    p->uiPass=[MTLRenderPassDescriptor renderPassDescriptor];
    // NewFrame needs only the attachment format, not an acquired drawable.
    if(!p->uiFormatTexture) {
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:p->layer.pixelFormat width:1 height:1 mipmapped:NO];
        descriptor.usage=MTLTextureUsageRenderTarget;
        p->uiFormatTexture=[p->device newTextureWithDescriptor:descriptor];
    }
    p->uiPass.colorAttachments[0].texture=p->uiFormatTexture;
    ImGui_ImplMetal_NewFrame(p->uiPass);
}
bool Context::beginFrame() {
    resize();
    if(p->width<=0 || p->height<=0) return false;
    p->drawable=[p->layer nextDrawable];
    if(!p->drawable) return false;
    p->command=[p->queue commandBuffer];
    p->command.label=@"nijigenerate Metal frame";
    return true;
}
void Context::present(ImDrawData* data) {
    auto pass=p->uiPass;
    pass.colorAttachments[0].texture=p->drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
    auto encoder=[p->command renderCommandEncoderWithDescriptor:pass];
    encoder.label=@"Editor UI";
    ImGui_ImplMetal_RenderDrawData(data,p->command,encoder);
    [encoder endEncoding];
    [p->command presentDrawable:p->drawable];
    [p->command commit];
    p->last=p->command; p->command=nil; p->drawable=nil;
}
bool Context::recording() const { return p->command!=nil; }
void Context::immediate(const std::function<void()>& fn) {
    if(recording()) { fn(); return; }
    @autoreleasepool {
        idle();
        p->command=[p->queue commandBuffer];
        try { fn(); } catch(...) { p->command=nil; throw; }
        [p->command commit]; [p->command waitUntilCompleted];
        auto done=p->command; p->command=nil; checkCommand(done);
    }
}
Image Context::createImage(uint32_t w,uint32_t h,uint32_t levels,bool depth,bool memoryless) {
    @autoreleasepool {
        if(!w || !h || w>uint32_t(maxTextureSize()) || h>uint32_t(maxTextureSize()))
            throw std::invalid_argument("Invalid Metal texture dimensions");
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:depth?MTLPixelFormatDepth32Float_Stencil8:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:levels>1];
        d.mipmapLevelCount=levels;
        d.storageMode=memoryless?MTLStorageModeMemoryless:MTLStorageModePrivate;
        d.usage=MTLTextureUsageRenderTarget | (depth?0:MTLTextureUsageShaderRead);
        return {retainMetal([p->device newTextureWithDescriptor:d]),w,h,levels};
    }
}
void Context::destroy(Image& image) { image={}; }
void Context::upload(Image& image,const std::vector<uint8_t>& bytes) {
    if(bytes.size()!=size_t(image.width)*image.height*4) throw std::invalid_argument("Metal texture upload size mismatch");
    immediate([&] {
        const auto rowBytes=(size_t(image.width)*4+255)&~size_t(255);
        auto staging=[p->device newBufferWithLength:rowBytes*image.height options:MTLResourceStorageModeShared];
        if(!staging) throw std::runtime_error("Metal texture staging allocation failed");
        for(uint32_t y=0;y<image.height;++y)
            std::memcpy(static_cast<uint8_t*>(staging.contents)+y*rowBytes,bytes.data()+size_t(y)*image.width*4,size_t(image.width)*4);
        auto blit=[p->command blitCommandEncoder];
        [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:rowBytes sourceBytesPerImage:rowBytes*image.height sourceSize:MTLSizeMake(image.width,image.height,1) toTexture:native(image) destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        if(image.mipLevels>1) [blit generateMipmapsForTexture:native(image)];
        [blit endEncoding];
    });
}
void Context::generateMipmaps(Image& image) {
    if(image.mipLevels<2) return;
    immediate([&] { auto blit=[p->command blitCommandEncoder]; [blit generateMipmapsForTexture:native(image)]; [blit endEncoding]; });
}
std::vector<uint8_t> Context::read(Image& image) {
    @autoreleasepool {
        if(recording()) throw std::logic_error("Readback must occur outside frame submission");
        idle();
        const auto rowBytes=(size_t(image.width)*4+255)&~size_t(255);
        auto buffer=[p->device newBufferWithLength:rowBytes*image.height options:MTLResourceStorageModeShared];
        if(!buffer) throw std::runtime_error("Metal readback allocation failed");
        auto command=[p->queue commandBuffer];
        auto blit=[command blitCommandEncoder];
        [blit copyFromTexture:native(image) sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(image.width,image.height,1) toBuffer:buffer destinationOffset:0 destinationBytesPerRow:rowBytes destinationBytesPerImage:rowBytes*image.height];
        [blit endEncoding]; [command commit]; [command waitUntilCompleted]; checkCommand(command);
        std::vector<uint8_t> bytes(size_t(image.width)*image.height*4);
        for(uint32_t y=0;y<image.height;++y)
            std::memcpy(bytes.data()+size_t(y)*image.width*4,static_cast<const uint8_t*>(buffer.contents)+y*rowBytes,size_t(image.width)*4);
        return bytes;
    }
}
ImTextureID Context::texture(Image& image) { return ImTextureID(reinterpret_cast<uintptr_t>(image.owner.get())); }
int Context::width() const { return p->width; }
int Context::height() const { return p->height; }
int Context::maxTextureSize() const { return 16384; }
std::string Context::deviceName() const { return p->device.name.UTF8String; }
}
