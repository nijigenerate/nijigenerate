// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
// Packet construction follows the editor's Vulkan backend; encoding uses Metal directly.
#include "backend.hpp"
#include "context_internal.hpp"
#include "emitter.hpp"
#include "difference.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <core/render/backend_queue.hpp>
#include <core/render/shared_deform_buffer.hpp>
#include <core/runtime_state.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace nijigenerate::backend::metal {
namespace core=nicxlive::core;
using namespace nicxlive::core::nodes;
namespace {
struct Vertex { float x,y,u,v,z=0; };
struct Material { float tint[4]{1,1,1,1}; float screen[4]{}; float emission[4]{}; };
struct DrawParameters { Material material; uint32_t mode,kind,offscreen,padding=0; };
static_assert(sizeof(DrawParameters)==64);
Vec3 transformClip(const Mat4 &m, const Vec3 &v) {
  float in[4] = {v.x, v.y, v.z, 1}, out[4]{};
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      out[row] += m[row][col] * in[col];
  float inv = out[3] == 0 || std::isnan(out[3]) ? 1.f : 1.f / out[3];
  return {out[0] * inv, out[1] * inv, out[2] * inv};
}
bool advanced(BlendMode m) {
  auto n = int(m);
  return (n >= 1 && n <= 6) || (n >= 9 && n <= 13);
}
void blendState(MTLRenderPipelineColorAttachmentDescriptor* s,BlendMode mode) {
    s.blendingEnabled=YES;
    s.sourceRGBBlendFactor=s.sourceAlphaBlendFactor=MTLBlendFactorOne;
    s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
    switch(mode) {
    case BlendMode::Multiply: s.sourceRGBBlendFactor=MTLBlendFactorDestinationColor; s.sourceAlphaBlendFactor=MTLBlendFactorDestinationAlpha; break;
    case BlendMode::Screen: s.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceColor; s.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha; break;
    case BlendMode::Darken: s.rgbBlendOperation=MTLBlendOperationMin; s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::Lighten: s.rgbBlendOperation=MTLBlendOperationMax; s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::ColorDodge: s.sourceRGBBlendFactor=MTLBlendFactorDestinationColor; s.sourceAlphaBlendFactor=MTLBlendFactorDestinationAlpha; s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::LinearDodge: s.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceColor; break;
    case BlendMode::AddGlow: s.destinationRGBBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::Exclusion: s.sourceRGBBlendFactor=MTLBlendFactorOneMinusDestinationColor; s.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceColor; s.destinationAlphaBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::Subtract: s.rgbBlendOperation=MTLBlendOperationReverseSubtract; s.sourceRGBBlendFactor=MTLBlendFactorOneMinusDestinationColor; s.sourceAlphaBlendFactor=MTLBlendFactorOneMinusDestinationAlpha; s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorOne; break;
    case BlendMode::Inverse: s.sourceRGBBlendFactor=MTLBlendFactorOneMinusDestinationColor; s.sourceAlphaBlendFactor=MTLBlendFactorOneMinusDestinationAlpha; break;
    case BlendMode::DestinationIn: s.sourceRGBBlendFactor=s.sourceAlphaBlendFactor=MTLBlendFactorZero; s.destinationRGBBlendFactor=s.destinationAlphaBlendFactor=MTLBlendFactorSourceAlpha; break;
    case BlendMode::ClipToLower: s.sourceRGBBlendFactor=s.sourceAlphaBlendFactor=MTLBlendFactorDestinationAlpha; break;
    case BlendMode::SliceFromLower: s.sourceRGBBlendFactor=s.sourceAlphaBlendFactor=MTLBlendFactorZero; break;
    default: break;
    }
}
}
struct Backend::Impl {
    Context& c=context();
    struct Texture {
        Image image;
        id<MTLSamplerState> sampler;
        int channels=4;
        bool stencil=false,mipmaps=true,uploaded=false;
        core::Filtering filtering=core::Filtering::Linear;
        core::Wrapping wrapping=core::Wrapping::Clamp;
        float anisotropy=1;
    };
    struct Target { std::array<uint32_t,3> colors{}; Image stencil; uint32_t width{},height{}; };
  struct Batch {
    Target *target{};
    uint32_t first{}, count{};
    int32_t base{};
    std::array<uint32_t, 3> textures{};
    Material material;
    BlendMode blend = BlendMode::Normal;
    int mask = 0;
    int kind = 0;
    uint32_t reference = 1;
    int clearStencil = -1;
    bool clear = false;
    bool depth = false;
    bool fixedBlend = false;
    Vec4 clearColor{};
    bool customClear = false;
  };
    bool spoutTarget=false;
    std::map<uint32_t,Texture> textures;
    std::map<core::RenderResourceHandle,std::vector<uint16_t>> indices;
    uint32_t nextTexture=1,nextIndex=1;
    std::unique_ptr<Target> root,blurA,blurB;
    struct Composite { std::weak_ptr<core::DynamicCompositeSurface> owner; std::unique_ptr<Target> target; };
    std::map<const core::DynamicCompositeSurface*,Composite> composites;
    std::map<uint32_t,std::unique_ptr<Target>> targets;
    std::vector<Vec3> debugPositions;
    std::vector<uint16_t> debugIndices;
    float pointSize=4,lineWidth=1;
    std::vector<Target*> stack;
    Target* target{};
    std::vector<Vertex> vertices;
    std::vector<uint16_t> elements;
    std::vector<Batch> batches;
    uint32_t white{},black{};
    int mask=0;
    uint32_t reference=1;
    bool lighting=false,postprocessingRequested=false,scenePending=false,sceneEnded=false;
    bool differenceEnabled=false,differenceReady=false,maskWritten=false,blurRequested=false;
    core::DifferenceEvaluationRegion differenceRegion{};
    DifferenceReduction difference;
    struct Pipeline { id<MTLRenderPipelineState> render; id<MTLDepthStencilState> depth; };
    std::map<uint64_t,Pipeline> pipelines;
    id<MTLSamplerState> sampler;
    const bool profile=[] { auto s=std::getenv("NIJIGENERATE_METAL_PROFILE"); return s && std::strcmp(s,"0"); }();
    uint64_t profileFrames=0,profileDraws=0,profilePasses=0;
    void initialize() {
        auto d=[MTLSamplerDescriptor new]; d.minFilter=d.magFilter=MTLSamplerMinMagFilterLinear;
        d.mipFilter=MTLSamplerMipFilterLinear; d.sAddressMode=d.tAddressMode=MTLSamplerAddressModeClampToEdge;
        sampler=[c.p->device newSamplerStateWithDescriptor:d];
    }
    void destroyTarget(Target& t) { t={}; }
    std::unique_ptr<Target> makeTarget(const std::array<uint32_t,3>& ids,bool=false) {
        auto t=std::make_unique<Target>(); t->colors=ids;
        const auto& image=textures.at(ids[0]).image; t->width=image.width; t->height=image.height;
        t->stencil=c.createImage(t->width,t->height,1,true); return t;
    }
    Pipeline pipeline(const Batch& b,bool mrt) {
        uint64_t key=uint64_t(b.blend)|(uint64_t(b.kind)<<8)|(uint64_t(b.mask)<<16)|(uint64_t(b.depth)<<20)|(uint64_t(mrt)<<21);
        if(auto it=pipelines.find(key);it!=pipelines.end()) return it->second;
        auto d=[MTLRenderPipelineDescriptor new];
        d.label=@"Model / editor geometry";
        d.vertexFunction=[c.p->library newFunctionWithName:@"sceneVertex"];
        d.fragmentFunction=[c.p->library newFunctionWithName:mrt?@"sceneFragment":@"singleFragment"];
        auto vd=[MTLVertexDescriptor vertexDescriptor];
        vd.attributes[0].format=MTLVertexFormatFloat2; vd.attributes[0].offset=0; vd.attributes[0].bufferIndex=0;
        vd.attributes[1].format=MTLVertexFormatFloat2; vd.attributes[1].offset=8; vd.attributes[1].bufferIndex=0;
        vd.attributes[2].format=MTLVertexFormatFloat; vd.attributes[2].offset=16; vd.attributes[2].bufferIndex=0;
        vd.layouts[0].stride=sizeof(Vertex); d.vertexDescriptor=vd;
        d.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;
        d.stencilAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;
        for(unsigned i=0;i<(mrt?3u:1u);++i) {
            auto a=d.colorAttachments[i]; a.pixelFormat=MTLPixelFormatRGBA8Unorm;
            blendState(a,b.blend);
            if(i==0 && advanced(b.blend)) a.blendingEnabled=NO;
            a.writeMask=b.mask==1?MTLColorWriteMaskNone:MTLColorWriteMaskAll;
        }
        auto a=d.colorAttachments[0];
        if(b.kind==9 || b.kind==13) a.blendingEnabled=NO;
        if(b.kind==10) {
            a.sourceRGBBlendFactor=MTLBlendFactorDestinationColor; a.destinationRGBBlendFactor=MTLBlendFactorZero;
            a.sourceAlphaBlendFactor=MTLBlendFactorZero; a.destinationAlphaBlendFactor=MTLBlendFactorOne;
        }
        if(b.kind==11) {
            a.sourceRGBBlendFactor=a.destinationRGBBlendFactor=MTLBlendFactorOne;
            a.sourceAlphaBlendFactor=MTLBlendFactorZero; a.destinationAlphaBlendFactor=MTLBlendFactorOne;
        }
        NSError* error=nil; Pipeline result;
        result.render=[c.p->device newRenderPipelineStateWithDescriptor:d error:&error];
        if(!result.render) throw std::runtime_error(error.localizedDescription.UTF8String);
        auto ds=[MTLDepthStencilDescriptor new];
        ds.depthCompareFunction=b.depth?MTLCompareFunctionLess:MTLCompareFunctionAlways;
        ds.depthWriteEnabled=b.depth;
        if(b.mask) {
            auto stencil=[MTLStencilDescriptor new];
            stencil.stencilCompareFunction=b.mask==1?MTLCompareFunctionAlways:MTLCompareFunctionEqual;
            stencil.depthStencilPassOperation=b.mask==1?MTLStencilOperationReplace:MTLStencilOperationKeep;
            stencil.readMask=255; stencil.writeMask=b.mask==1?255:0;
            ds.frontFaceStencil=ds.backFaceStencil=stencil;
        }
        result.depth=[c.p->device newDepthStencilStateWithDescriptor:ds];
        pipelines.emplace(key,result); return result;
    }
    void record() {
        if(!c.recording()) throw std::logic_error("Metal recording requires a command buffer");
        auto command=c.p->command;
        id<MTLBuffer> vertexBuffer=nil,indexBuffer=nil;
        if(!vertices.empty()) vertexBuffer=[c.p->device newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(Vertex) options:MTLResourceStorageModeShared];
        if(!elements.empty()) indexBuffer=[c.p->device newBufferWithBytes:elements.data() length:elements.size()*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        Target* active=nullptr; bool activeMrt=false;
        id<MTLRenderCommandEncoder> encoder=nil;
        auto end=[&] { if(encoder) { [encoder endEncoding]; encoder=nil; active=nullptr; } };
        uint64_t draws=0,passes=0;
        for(const auto& b:batches) {
            auto& t=*b.target;
            if(b.kind==14) { end(); for(auto id:t.colors) if(id) c.generateMipmaps(textures.at(id).image); continue; }
            const bool mrt=b.kind<9 && t.colors[1] && t.colors[1]!=t.colors[0] && t.colors[2] && t.colors[2]!=t.colors[0];
            Image lightingSnapshot;
            if(b.kind==13 || (b.count && advanced(b.blend) && !c.advancedBlendSupported)) {
                end(); if(b.kind==13) c.generateMipmaps(textures.at(t.colors[1]).image);
                lightingSnapshot=c.createImage(t.width,t.height);
                auto blit=[command blitCommandEncoder];
                [blit copyFromTexture:native(textures.at(t.colors[0]).image) sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(t.width,t.height,1) toTexture:native(lightingSnapshot) destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
                [blit endEncoding];
            }
            const bool restart=active!=&t || activeMrt!=mrt || b.clear;
            if(restart) {
                end(); auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
                for(unsigned i=0;i<(mrt?3u:1u);++i) {
                    auto a=pass.colorAttachments[i]; a.texture=native(textures.at(t.colors[i]).image);
                    a.loadAction=b.clear?MTLLoadActionClear:MTLLoadActionLoad; a.storeAction=MTLStoreActionStore;
                    Vec4 color{};
                    if(i==0 && &t==root.get()) core::inGetClearColor(color.x,color.y,color.z,color.w);
                    if(b.customClear) color=b.clearColor;
                    a.clearColor=MTLClearColorMake(color.x,color.y,color.z,color.w);
                }
                pass.depthAttachment.texture=pass.stencilAttachment.texture=native(t.stencil);
                pass.depthAttachment.loadAction=b.clear?MTLLoadActionClear:MTLLoadActionLoad;
                pass.depthAttachment.storeAction=MTLStoreActionStore; pass.depthAttachment.clearDepth=1;
                pass.stencilAttachment.loadAction=b.clear || b.clearStencil>=0?MTLLoadActionClear:MTLLoadActionLoad;
                pass.stencilAttachment.storeAction=MTLStoreActionStore; pass.stencilAttachment.clearStencil=std::max(0,b.clearStencil);
                encoder=[command renderCommandEncoderWithDescriptor:pass];
                encoder.label=@"Model / editor target";
                [encoder setViewport:MTLViewport{0,0,double(t.width),double(t.height),0,1}];
                [encoder setScissorRect:MTLScissorRect{0,0,t.width,t.height}];
                active=&t; activeMrt=mrt; ++passes;
            }
            if(!restart && b.clearStencil>=0) {
                // Clear stencil inside the existing tile pass. Restarting an
                // encoder here would store/reload all three color planes for
                // every mask, despite changing no color pixels.
                Batch clear=b; clear.kind=8; clear.mask=1; clear.depth=false;
                clear.blend=BlendMode::Normal;
                const auto state=pipeline(clear,mrt);
                [encoder setRenderPipelineState:state.render];
                [encoder setDepthStencilState:state.depth];
                [encoder setStencilReferenceValue:uint32_t(b.clearStencil)];
                const Vertex triangle[]={{-1,-1,0,0},{3,-1,0,0},{-1,3,0,0}};
                const DrawParameters params{Material{},0,8,uint32_t(&t!=root.get())};
                [encoder setVertexBytes:triangle length:sizeof(triangle) atIndex:0];
                [encoder setVertexBytes:&params length:sizeof(params) atIndex:1];
                [encoder setFragmentBytes:&params length:sizeof(params) atIndex:0];
                for(unsigned i=0;i<4;++i) {
                    [encoder setFragmentTexture:native(textures.at(white).image) atIndex:i];
                    [encoder setFragmentSamplerState:sampler atIndex:i];
                }
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                ++draws;
            }
            if(!b.count) continue;
            const auto state=pipeline(b,mrt);
            [encoder setRenderPipelineState:state.render]; [encoder setDepthStencilState:state.depth];
            [encoder setStencilReferenceValue:b.reference];
            DrawParameters params{b.material,uint32_t(b.blend),uint32_t(b.kind),uint32_t(&t!=root.get())};
            [encoder setVertexBytes:&params length:sizeof(params) atIndex:1];
            [encoder setFragmentBytes:&params length:sizeof(params) atIndex:0];
            for(unsigned i=0;i<3;++i) {
                auto id=b.textures[i]; if(!id || !textures.contains(id)) id=i==0?white:black;
                const auto& texture=textures.at(id);
                [encoder setFragmentTexture:i==0 && b.kind==13 && lightingSnapshot.owner?native(lightingSnapshot):native(texture.image) atIndex:i];
                [encoder setFragmentSamplerState:texture.sampler?texture.sampler:sampler atIndex:i];
            }
            [encoder setFragmentTexture:lightingSnapshot.owner?native(lightingSnapshot):native(textures.at(black).image) atIndex:3];
            [encoder setVertexBuffer:vertexBuffer offset:size_t(b.base)*sizeof(Vertex) atIndex:0];
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:b.count indexType:MTLIndexTypeUInt16 indexBuffer:indexBuffer indexBufferOffset:size_t(b.first)*sizeof(uint16_t)];
            ++draws;
        }
        end();
        if(profile) {
            profileDraws+=draws; profilePasses+=passes;
            if(++profileFrames==120) {
                std::fprintf(stderr,"[metal-gpu] draws=%.1f passes=%.1f framebuffer_fetch=%d\n",profileDraws/120.,profilePasses/120.,int(c.advancedBlendSupported));
                profileFrames=profileDraws=profilePasses=0;
            }
        }
    }
};
Backend &backend() {
  auto b = std::dynamic_pointer_cast<Backend>(core::getCurrentRenderBackend());
  if (!b)
    throw std::logic_error("Metal model backend is not active");
  return *b;
}
ImTextureID uiTexture(uint32_t id) { return id ? backend().uiTexture(id) : 0; }
Backend::Backend() : p(std::make_unique<Impl>()) {}
Backend::~Backend() { shutdown(); }
void Backend::shutdown() { if(!p) return; p->c.idle(); p.reset(); }
void Backend::initializeRenderer() {
  p->initialize();
  p->white = createTexture({255, 255, 255, 255}, 1, 1, 4, false);
  p->black = createTexture({0, 0, 0, 0}, 1, 1, 4, false);
  resizeViewportTargets(p->c.width(), p->c.height());
}
void Backend::createDrawableBuffers(nicxlive::core::RenderResourceHandle &id) {
  if (!id)
    id = p->nextIndex++;
}
void Backend::disposeDrawableBuffers(nicxlive::core::RenderResourceHandle id) {
  if (p)
    p->indices.erase(id);
}
void Backend::uploadDrawableIndices(nicxlive::core::RenderResourceHandle id,
                                    const std::vector<uint16_t> &values) {
  p->indices[id] = values;
}
uint32_t Backend::createTexture(const std::vector<uint8_t> &data, int w, int h,
                                int channels, bool stencil) {
  if (w == 0 && h == 0 && data.empty())
    return createTexture({0,0,0,0},1,1,4,false);
  if (w <= 0 || h <= 0 || channels < 1 || channels > 4)
    throw std::runtime_error("Invalid texture dimensions");
  auto id = p->nextTexture++;
  Impl::Texture t;
  t.channels = channels;
  t.stencil = stencil;
  t.image = p->c.createImage(w, h);
  p->textures.emplace(id, std::move(t));
  updateTexture(id, data, w, h, channels);
  return id;
}
void Backend::updateTexture(uint32_t id, const std::vector<uint8_t> &data,
                            int w, int h, int channels) {
  auto &t = p->textures.at(id);
  if(t.uploaded) flush();
  if (w != int(t.image.width) || h != int(t.image.height)) {
    p->c.idle();
    p->c.destroy(t.image);
    t.image = p->c.createImage(w, h);
  }
  std::vector<uint8_t> rgba(size_t(w) * h * 4, 0);
  if (!data.empty()) {
    if (data.size() < size_t(w) * h * channels)
      throw std::runtime_error("Truncated texture pixels");
    for (size_t i = 0; i < size_t(w) * h; ++i) {
      rgba[i * 4] = data[i * channels];
      rgba[i * 4 + 1] = channels > 1 ? data[i * channels + 1] : rgba[i * 4];
      rgba[i * 4 + 2] = channels > 2 ? data[i * channels + 2] : rgba[i * 4];
      rgba[i * 4 + 3] = channels > 3 ? data[i * channels + 3] : 255;
    }
  }
  p->c.upload(t.image, rgba);
  t.uploaded=true;
}
void Backend::setTextureParams(uint32_t id, core::Filtering filtering,
                               core::Wrapping wrapping, float anisotropy) {
    auto& t=p->textures.at(id); t.filtering=filtering; t.wrapping=wrapping;
    t.anisotropy=std::clamp(anisotropy,1.f,p->c.maxAnisotropy);
    auto d=[MTLSamplerDescriptor new];
    d.minFilter=d.magFilter=filtering==core::Filtering::Linear?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;
    d.mipFilter=t.mipmaps?(filtering==core::Filtering::Linear?MTLSamplerMipFilterLinear:MTLSamplerMipFilterNearest):MTLSamplerMipFilterNotMipmapped;
    d.sAddressMode=d.tAddressMode=d.rAddressMode=wrapping==core::Wrapping::Clamp?MTLSamplerAddressModeClampToEdge:wrapping==core::Wrapping::Mirror?MTLSamplerAddressModeMirrorRepeat:MTLSamplerAddressModeRepeat;
    d.maxAnisotropy=NSUInteger(t.anisotropy); t.sampler=[p->c.p->device newSamplerStateWithDescriptor:d];
}
void Backend::disposeTexture(uint32_t id) {
  if (!p)
    return;
  flush();
  if (auto target = p->targets.find(id); target != p->targets.end()) {
    p->c.idle(); p->destroyTarget(*target->second); p->targets.erase(target);
  }
  auto it = p->textures.find(id);
  if (it == p->textures.end())
    return;
  p->c.idle();
  for (auto c = p->composites.begin(); c != p->composites.end();) {
    auto &ids = c->second.target->colors;
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
      p->destroyTarget(*c->second.target);
      c = p->composites.erase(c);
    } else
      ++c;
  }

  p->c.destroy(it->second.image);
  p->textures.erase(it);
}
std::vector<uint8_t> Backend::readTexture(uint32_t id) {
  flush();
  return p->c.read(p->textures.at(id).image);
}
ImTextureID Backend::uiTexture(uint32_t id) {
  if(!id) id=p->white;
  return p->c.texture(p->textures.at(id).image);
}
void Backend::resizeViewportTargets(int w, int h) {
  if (w <= 0 || h <= 0)
    return;
  bool sharedOutput = false;
  if (p->spoutTarget == sharedOutput && p->root && p->root->width == uint32_t(w) &&
      p->root->height == uint32_t(h))
    return;
  flush();
  p->c.idle();
  if (p->root) {
    auto ids = p->root->colors;
    p->destroyTarget(*p->root);
    p->root.reset();
    for (auto id : ids)
      disposeTexture(id);
  }
  std::array<uint32_t, 3> ids;
  for (auto &id : ids) {
    id = createTexture({}, w, h, 4, false);
  }
  p->spoutTarget = sharedOutput;
  generateTextureMipmap(ids[1]);
  p->root = p->makeTarget(ids, true);
  p->target = p->root.get();
}
void Backend::beginScene() {
  flush();
  p->scenePending = true;
  p->sceneEnded = false;
  p->postprocessingRequested=false;
  p->vertices.clear();
  p->elements.clear();
  p->batches.clear();
  p->stack.clear();
  p->target = p->root.get();
  p->mask = 0;
  Impl::Batch clear;
  clear.target = p->target;
  clear.clear = true;
  p->batches.push_back(clear);
  for (auto it = p->composites.begin(); it != p->composites.end();)
    if (it->second.owner.expired()) {
      p->destroyTarget(*it->second.target);
      it = p->composites.erase(it);
    } else
      ++it;
}
void Backend::endScene() { p->sceneEnded=true; }
void Backend::addBasicLightingPostProcess() { p->lighting = true; }
void Backend::postProcessScene() {
  if(p->lighting) { p->postprocessingRequested=true; p->scenePending=true; p->sceneEnded=true; }
}
void Backend::render() {
  if (p->batches.empty() && !(p->scenePending && p->sceneEnded)) return;
  if (p->scenePending && p->sceneEnded) {
  if(p->postprocessingRequested) {
    Impl::Batch light; light.target=p->root.get(); light.kind=13;
    light.base=int32_t(p->vertices.size()); light.first=uint32_t(p->elements.size()); light.count=6;
    light.textures[1]=p->root->colors[1];
    light.material.tint[0]=ngSceneAmbientLight.x; light.material.tint[1]=ngSceneAmbientLight.y; light.material.tint[2]=ngSceneAmbientLight.z;
    light.material.screen[0]=1.f/p->root->width; light.material.screen[1]=1.f/p->root->height;
    for(auto v:std::array<Vertex,4>{{{-1,-1,0,1},{1,-1,1,1},{1,1,1,0},{-1,1,0,0}}}) p->vertices.push_back(v);
    for(uint16_t i:{0,1,2,0,2,3}) p->elements.push_back(i);
    p->batches.push_back(light); p->postprocessingRequested=false;
  }
  p->scenePending = false;
  }
  if (p->blurRequested) {
    auto append = [&](Impl::Target *target, uint32_t source, float x, float y) {
      Impl::Batch b;
      b.target = target;
      b.base = int32_t(p->vertices.size());
      b.first = uint32_t(p->elements.size());
      b.count = 6;
      b.kind = 9;
      b.textures[0] = source;
      b.material.screen[0] = x;
      b.material.screen[1] = y;
      for (auto v : std::array<Vertex, 4>{
               {{-1, -1, 0, 0}, {1, -1, 1, 0}, {1, 1, 1, 1}, {-1, 1, 0, 1}}})
        p->vertices.push_back(v);
      for (uint16_t i : {0, 1, 2, 0, 2, 3})
        p->elements.push_back(i);
      p->batches.push_back(b);
    };
    auto a = p->blurA.get(), b = p->blurB.get();
    append(a, renderImageHandle(), 0, 0);
    append(b, a->colors[0], 2.2f / a->width, 0);
    append(a, b->colors[0], 0, 2.2f / a->height);
    append(b, a->colors[0], 3.6f / a->width, 0);
    append(a, b->colors[0], 0, 3.6f / a->height);
  }
  p->record();
  p->blurRequested = false;
  p->batches.clear();
  p->vertices.clear();
  p->elements.clear();
}
ImTextureID Backend::overlayBlurTexture() {
  auto w = std::max(1u, p->root->width / 4),
       h = std::max(1u, p->root->height / 4);
  if (!p->blurA || p->blurA->width != w || p->blurA->height != h) {
    p->c.idle();
    for (auto ptr : {&p->blurA, &p->blurB}) {
      if (*ptr) {
        auto id = (*ptr)->colors[0];
        p->destroyTarget(**ptr);
        ptr->reset();
        disposeTexture(id);
      }
      auto id = createTexture({}, w, h, 4, false);
      *ptr = p->makeTarget({id, id, id});
    }
  }
  p->blurRequested = true;
  return uiTexture(p->blurA->colors[0]);
}
Image &Backend::sceneImage() {
  return p->textures.at(renderImageHandle()).image;
}
nicxlive::core::RenderResourceHandle Backend::renderImageHandle() { return p->root->colors[0]; }
nicxlive::core::RenderResourceHandle Backend::mainAlbedoHandle() { return p->root->colors[0]; }
nicxlive::core::RenderResourceHandle Backend::mainEmissiveHandle() { return p->root->colors[1]; }
nicxlive::core::RenderResourceHandle Backend::mainBumpHandle() { return p->root->colors[2]; }
void Backend::dumpViewport(std::vector<uint8_t> &out, int w, int h) {
  if (w != int(p->root->width) || h != int(p->root->height))
    throw std::runtime_error("Viewport readback dimensions mismatch");
  out = readTexture(renderImageHandle());
  const auto stride=size_t(w)*4;
  for(int y=0;y<h/2;++y) std::swap_ranges(out.begin()+size_t(y)*stride,
      out.begin()+size_t(y+1)*stride,out.begin()+size_t(h-1-y)*stride);
}
void Backend::drawPuppet(core::Puppet &puppet) {
  using namespace core::render;
  auto host = core::getCurrentRenderBackend();
  auto queue = std::make_shared<QueueRenderBackend>();
  puppet.setRenderBackend(queue);
  core::setCurrentRenderBackend(host);
  puppet.draw();
  for (auto &cmd : queue->queue) {
    switch (cmd.kind) {
    case RenderCommandKind::DrawPart:
      drawPartPacket(cmd.partPacket);
      break;
    case RenderCommandKind::BeginMask:
      beginMask(cmd.usesStencil);
      break;
    case RenderCommandKind::ApplyMask:
      applyMask(cmd.maskApplyPacket);
      break;
    case RenderCommandKind::BeginMaskContent:
      beginMaskContent();
      break;
    case RenderCommandKind::EndMask:
      endMask();
      break;
    case RenderCommandKind::BeginDynamicComposite:
      beginDynamicComposite(cmd.dynamicPass);
      break;
    case RenderCommandKind::EndDynamicComposite:
      endDynamicComposite(cmd.dynamicPass);
      break;
    }
  }
  queue->clear();
}
void Backend::drawPartPacket(const PartDrawPacket &packet) {
  if (!packet.renderable || !packet.vertexCount || !packet.indexCount)
    return;
  auto ix = p->indices.find(packet.indexBuffer);
  if (ix == p->indices.end())
    throw std::runtime_error("Missing Metal drawable indices");
  auto v = core::render::sharedVertexBufferData().rawStorage(),
       u = core::render::sharedUvBufferData().rawStorage(),
       d = core::render::sharedDeformBufferData().rawStorage();
  auto count = packet.vertexCount;
  if (packet.vertexOffset + packet.vertexAtlasStride + count > v.length ||
      packet.uvOffset + packet.uvAtlasStride + count > u.length ||
      packet.deformOffset + packet.deformAtlasStride + count > d.length)
    throw std::runtime_error("Drawable atlas bounds exceeded");
  Impl::Batch b;
  b.target = p->target;
  b.base = int32_t(p->vertices.size());
  b.first = uint32_t(p->elements.size());
  b.count = std::min<uint32_t>(packet.indexCount, uint32_t(ix->second.size()));
  b.mask = p->mask;
  b.reference = p->reference;
  b.blend = p->mask == 1 ? BlendMode::Normal : packet.blendMode;
  auto matrix = packet.renderMatrix * packet.modelMatrix;
  for (uint32_t i = 0; i < count; ++i) {
    float x = v.ptr[packet.vertexOffset + i] + d.ptr[packet.deformOffset + i] -
              packet.origin.x;
    float y = v.ptr[packet.vertexOffset + packet.vertexAtlasStride + i] +
              d.ptr[packet.deformOffset + packet.deformAtlasStride + i] -
              packet.origin.y;
    auto clip = transformClip(matrix, {x, y, 0});
    p->vertices.push_back({clip.x, clip.y, u.ptr[packet.uvOffset + i],
                           u.ptr[packet.uvOffset + packet.uvAtlasStride + i]});
  }
  p->elements.insert(p->elements.end(), ix->second.begin(),
                     ix->second.begin() + b.count);
  std::copy_n(packet.textureBackendIds, 3, b.textures.begin());
  if (auto node = packet.node.lock())
    for (int i = 0; i < 3; ++i)
      if (node->textures[i])
        b.textures[i] = node->textures[i]->backendId();
  b.material.tint[0] = packet.clampedTint.x;
  b.material.tint[1] = packet.clampedTint.y;
  b.material.tint[2] = packet.clampedTint.z;
  b.material.tint[3] = packet.opacity;
  b.material.screen[0] = packet.clampedScreen.x;
  b.material.screen[1] = packet.clampedScreen.y;
  b.material.screen[2] = packet.clampedScreen.z;
  b.material.screen[3] = packet.emissionStrength;
  for (const auto &[_, composite] : p->composites)
    if (composite.target->colors[0] == b.textures[0] &&
        composite.target->colors[1] == b.textures[1]) {
      b.material.emission[0] = 1;
      break;
    }
  b.kind = p->mask == 1 ? 6 : packet.isMask ? 3 : p->mask == 2 ? 5 : 0;
  if (packet.isMask || p->mask == 1)
    b.material.screen[3] = packet.maskThreshold;
  if (p->mask == 1)
    p->maskWritten = true;
  p->batches.push_back(b);
}
void Backend::beginMask(bool normal) {
  p->maskWritten = false;
  p->mask = 1;
  Impl::Batch b;
  b.target = p->target;
  b.clearStencil = normal ? 0 : 1;
  p->batches.push_back(b);
}
void Backend::beginMaskContent() {
  p->mask = p->maskWritten ? 2 : 0;
  p->reference = 1;
}
void Backend::endMask() { p->mask = 0; }
void Backend::applyMask(const MaskApplyPacket &m) {
  if (p->mask != 1)
    return;
  p->reference = m.isDodge ? 0 : 1;
  if (m.kind == MaskDrawableKind::Part) {
    drawPartPacket(m.partPacket);
    return;
  }
  const auto &mask = m.maskPacket;
  auto v = core::render::sharedVertexBufferData().rawStorage(),
       d = core::render::sharedDeformBufferData().rawStorage();
  auto it = p->indices.find(mask.indexBuffer);
  if (it == p->indices.end() || !mask.vertexCount)
    return;
  Impl::Batch b;
  b.target = p->target;
  b.base = int32_t(p->vertices.size());
  b.first = uint32_t(p->elements.size());
  b.count = uint32_t(it->second.size());
  b.mask = 1;
  b.reference = p->reference;
  b.kind = 8;
  p->maskWritten = true;
  for (uint32_t i = 0; i < mask.vertexCount; ++i) {
    auto clip = transformClip(
        mask.mvp, {v.ptr[mask.vertexOffset + i] + d.ptr[mask.deformOffset + i] -
                       mask.origin.x,
                   v.ptr[mask.vertexOffset + mask.vertexAtlasStride + i] +
                       d.ptr[mask.deformOffset + mask.deformAtlasStride + i] -
                       mask.origin.y,
                   0});
    p->vertices.push_back({clip.x, clip.y, 0, 0});
  }
  p->elements.insert(p->elements.end(), it->second.begin(), it->second.end());
  p->batches.push_back(b);
}
void Backend::beginDynamicComposite(core::DynamicCompositePass &pass) {
  if (!pass.surface)
    throw std::runtime_error("Dynamic composite missing surface");
  std::array<uint32_t, 3> ids{};
  for (int i = 0; i < 3; ++i)
    if (pass.surface->textures[i])
      ids[i] = pass.surface->textures[i]->backendId();
  if (!ids[0])
    throw std::runtime_error("Dynamic composite missing color target");
  auto &entry = p->composites[pass.surface.get()];
  if (!entry.target || entry.target->colors != ids) {
    if (entry.target)
      p->destroyTarget(*entry.target);
    entry.owner = pass.surface;
    entry.target = p->makeTarget(ids);
  }
  p->stack.push_back(p->target);
  p->target = entry.target.get();
  Impl::Batch b;
  b.target = p->target;
  b.clear = true;
  p->batches.push_back(b);
}
void Backend::endDynamicComposite(core::DynamicCompositePass &pass) {
  if (p->stack.empty())
    throw std::runtime_error("Unbalanced dynamic composite");
  if(!pass.autoScaled) {
    Impl::Batch mip; mip.target=p->target; mip.kind=14; p->batches.push_back(mip);
  }
  p->target = p->stack.back();
  p->stack.pop_back();
}
void Backend::destroyDynamicComposite(
    const std::shared_ptr<core::DynamicCompositeSurface> &surface) {
  if (!p)
    return;
  auto it = p->composites.find(surface.get());
  if (it != p->composites.end()) {
    p->c.idle();
    p->destroyTarget(*it->second.target);
    p->composites.erase(it);
  }
}
void Backend::drawSprites(uint32_t texture, const float *data, size_t count,
                          const Mat4 &matrix) {
  if (!count)
    return;
  Impl::Batch b;
  b.target = p->target;
  b.base = int32_t(p->vertices.size());
  b.first = uint32_t(p->elements.size());
  b.count = uint32_t(count);
  b.textures[0] = texture;
  auto &image = p->textures.at(texture).image;
  for (size_t i = 0; i < count; ++i) {
    auto v = matrix.transformPoint({data[i * 8], data[i * 8 + 1], 0});
    p->vertices.push_back({v.x, v.y, data[i * 8 + 2] / image.width,
                           data[i * 8 + 3] / image.height});
    p->elements.push_back(uint16_t(i));
  }
  std::copy_n(data + 4, 4, b.material.tint);
  p->batches.push_back(b);
}

namespace {
struct TextureResource final : core::RenderTextureHandle { uint32_t id{}; };
std::shared_ptr<TextureResource> resource(Backend::TextureHandle h) {
  auto result = std::dynamic_pointer_cast<TextureResource>(h);
  if (!result) throw std::invalid_argument("Texture belongs to a different renderer");
  return result;
}
std::vector<uint8_t> copyBytes(core::SourceTextureBytes bytes) {
  return bytes.empty() ? std::vector<uint8_t>{} : std::vector<uint8_t>(bytes.data(), bytes.data()+bytes.size());
}
}
std::unique_ptr<core::RenderCommandEmitter> Backend::createCommandEmitter() { return std::make_unique<RenderQueue>(); }
Backend::TextureHandle Backend::createTextureHandle() { return std::make_shared<TextureResource>(); }
void Backend::destroyTextureHandle(TextureHandle h) { auto r=resource(h); disposeTexture(r->id); r->id=0; }
void Backend::bindTextureHandle(TextureHandle h, unsigned unit) { bindTextureId(resource(h)->id,unit); }
void Backend::bindTextureId(uint32_t id, uint32_t unit) {
  if (unit>31 || (id && !p->textures.contains(id))) throw std::invalid_argument("Invalid texture binding");
  // Bindings are captured by each explicit draw packet rather than global GPU state.
}
void Backend::uploadTextureData(TextureHandle h,int w,int height,int inChannels,int outChannels,bool stencil,Bytes bytes) {
  auto r=resource(h);
  if (!r->id) r->id=createTexture(copyBytes(bytes),w,height,inChannels,stencil);
  else uploadTextureData(r->id,w,height,inChannels,outChannels,stencil,copyBytes(bytes));
}
void Backend::uploadTextureData(uint32_t id,int w,int height,int inChannels,int,bool stencil,Bytes bytes) {
  uploadTextureData(id,w,height,inChannels,inChannels,stencil,copyBytes(bytes));
}
void Backend::uploadTextureData(uint32_t id,int w,int height,int inChannels,int,bool stencil,const std::vector<uint8_t>& bytes) {
  auto& t=p->textures.at(id); t.channels=inChannels; t.stencil=stencil;
  updateTexture(id,bytes,w,height,inChannels);
}
void Backend::updateTextureRegion(TextureHandle h,int x,int y,int w,int height,int channels,Bytes bytes) {
  auto id=resource(h)->id;
  auto& image=p->textures.at(id).image;
  if (x<0 || y<0 || w<0 || height<0 || x+w>int(image.width) || y+height>int(image.height) ||
      channels<1 || channels>4 || bytes.size()<size_t(w)*height*channels)
    throw std::invalid_argument("Invalid texture update region");
  auto pixels=readTexture(id);
  for (int row=0;row<height;++row) for(int col=0;col<w;++col) {
    auto source=bytes.data()+(size_t(row)*w+col)*channels;
    auto dest=pixels.data()+((size_t(y+row)*image.width)+x+col)*4;
    for(int c=0;c<4;++c) dest[c]=c<channels?source[c]:(c==3?255:source[0]);
  }
  p->c.upload(image,pixels);
}
std::size_t Backend::textureNativeHandle(TextureHandle h) { return resource(h)->id; }
std::size_t Backend::textureNativeHandle(uint32_t id) { return id; }
void Backend::readTextureData(uint32_t id,int channels,bool,std::vector<uint8_t>& out) {
  if(channels<1 || channels>4) throw std::invalid_argument("Invalid texture readback channels");
  auto rgba=readTexture(id); out.resize(rgba.size()/4*channels);
  for(size_t i=0;i<rgba.size()/4;++i) std::copy_n(rgba.data()+i*4,channels,out.data()+i*channels);
}
void Backend::readTextureData(uint32_t id,int channels,bool stencil,Bytes out) {
  std::vector<uint8_t> pixels; readTextureData(id,channels,stencil,pixels);
  if(out.size()<pixels.size()) throw std::invalid_argument("Texture readback destination is too small");
  std::copy(pixels.begin(),pixels.end(),out.data());
}
void Backend::readTextureData(TextureHandle h,int channels,bool stencil,Bytes out) { readTextureData(resource(h)->id,channels,stencil,out); }
void Backend::applyTextureFiltering(TextureHandle h,core::Filtering f,bool mipmaps) {
  auto id=resource(h)->id; auto& t=p->textures.at(id); t.mipmaps=mipmaps; setTextureParams(id,f,t.wrapping,t.anisotropy);
}
void Backend::applyTextureWrapping(TextureHandle h,core::Wrapping w) {
  auto id=resource(h)->id; auto& t=p->textures.at(id); setTextureParams(id,t.filtering,w,t.anisotropy);
}
void Backend::applyTextureAnisotropy(TextureHandle h,float value) {
  auto id=resource(h)->id; auto& t=p->textures.at(id); t.anisotropy=value; setTextureParams(id,t.filtering,t.wrapping,value);
}
float Backend::maxTextureAnisotropy() { return p->c.maxAnisotropy; }
void Backend::dumpViewport(core::common::SharedArray<uint8_t>& out,int w,int h) {
  std::vector<uint8_t> pixels; dumpViewport(pixels,w,h); out.resize(pixels.size()); std::copy(pixels.begin(),pixels.end(),out.data());
}
void Backend::uploadDrawableIndices(Handle h,core::common::SharedArray<uint16_t> values) {
  uploadDrawableIndices(h,std::vector<uint16_t>(values.begin(),values.end()));
}
bool Backend::supportsAdvancedBlend() { return p->c.advancedBlendSupported; }
bool Backend::supportsAdvancedBlendCoherent() { return p->c.advancedBlendCoherent; }
void Backend::setAdvancedBlendCoherent(bool enabled) {
  if(enabled && !supportsAdvancedBlendCoherent()) throw std::invalid_argument("Coherent advanced blend is unavailable");
}
void Backend::setLegacyBlendMode(BlendMode) {}
void Backend::setAdvancedBlendEquation(BlendMode) {}
void Backend::issueBlendBarrier() {} // Recorded at each dependent Metal attachment draw.
bool Backend::hasTexture(uint32_t id) const { return p && p->textures.contains(id); }
void Backend::flush() {
    if(!p || (p->batches.empty() && !(p->scenePending && p->sceneEnded))) return;
    p->c.immediate([&] { render(); });
}

void Backend::pushTarget(uint32_t id,Vec4 color,bool clear) {
  auto& target=p->targets[id];
  if(!target) target=p->makeTarget({id,id,id});
  p->stack.push_back(p->target); p->target=target.get();
  Impl::Batch b; b.target=p->target; b.clear=clear; b.customClear=true; b.clearColor=color; p->batches.push_back(b);
}
void Backend::popTarget() {
  if(p->stack.empty()) throw std::logic_error("Unbalanced Metal target scope");
  p->target=p->stack.back(); p->stack.pop_back();
}
void Backend::drawMesh(uint32_t texture,std::span<const Vec3> positions,std::span<const Vec2> uvs,
    std::span<const uint32_t> indices,const Mat4& matrix,Vec4 tint,Vec3 screen,bool depth,BlendMode blend) {
  if(indices.empty()) return;
  if(indices.size()%3) throw std::invalid_argument("Triangle index count must be a multiple of three");
  for(size_t first=0;first<indices.size();first+=65535) {
    auto count=std::min<size_t>(65535,indices.size()-first);
    Impl::Batch b; b.target=p->target; b.base=int32_t(p->vertices.size());
    b.first=uint32_t(p->elements.size()); b.count=uint32_t(count); b.depth=depth; b.blend=blend;
    b.textures[0]=texture?texture:p->white; b.material.tint[0]=tint.x; b.material.tint[1]=tint.y;
    b.material.tint[2]=tint.z; b.material.tint[3]=tint.w;
    b.material.screen[0]=screen.x; b.material.screen[1]=screen.y; b.material.screen[2]=screen.z;
    for(size_t i=0;i<count;++i) {
      auto index=indices[first+i];
      if(index>=positions.size() || (!uvs.empty() && index>=uvs.size())) throw std::out_of_range("Mesh index");
      auto clip=transformClip(matrix,positions[index]); auto uv=uvs.empty()?Vec2{}:uvs[index];
      p->vertices.push_back({clip.x,clip.y,uv.x,uv.y,clip.z}); p->elements.push_back(uint16_t(i));
    }
    p->batches.push_back(b);
  }
}
void Backend::initDebugRenderer() {}
void Backend::setDebugPointSize(float size) { p->pointSize=size; }
void Backend::setDebugLineWidth(float width) { p->lineWidth=width; }
void Backend::uploadDebugBuffer(const std::vector<Vec3>& positions,const std::vector<uint16_t>& indices) {
  p->debugPositions=positions; p->debugIndices=indices;
}
void Backend::uploadDebugBuffer(core::math::Vec3Array positions,core::common::SharedArray<uint16_t> indices) {
  p->debugPositions.resize(positions.size());
  for(size_t i=0;i<positions.size();++i) p->debugPositions[i]=positions.at(i);
  p->debugIndices.assign(indices.begin(),indices.end());
}
void Backend::drawDebugPoints(const Vec4& color,const Mat4& matrix) {
  const auto width=float(p->target->width), height=float(p->target->height);
  std::vector<Vec3> vertices; std::vector<uint32_t> indices;
  auto point=[&](size_t i) {
    if(i>=p->debugPositions.size()) throw std::out_of_range("Debug point index");
    auto v=transformClip(matrix,p->debugPositions[i]);
    const auto base=uint32_t(vertices.size()); vertices.push_back(v);
    constexpr uint32_t segments=16;
    for(uint32_t n=0;n<segments;++n) {
      auto a=float(n)*6.28318530718f/segments;
      vertices.push_back({v.x+std::cos(a)*p->pointSize/width,v.y+std::sin(a)*p->pointSize/height,v.z});
      indices.insert(indices.end(),{base,base+1+n,base+1+(n+1)%segments});
    }
  };
  if(p->debugIndices.empty()) for(size_t i=0;i<p->debugPositions.size();++i) point(i);
  else for(auto i:p->debugIndices) point(i);
  drawMesh(0,vertices,{},indices,Mat4::identity(),color);
}
void Backend::drawDebugLines(const Vec4& color,const Mat4& matrix) {
  const auto width=float(p->target->width),height=float(p->target->height);
  std::vector<Vec3> vertices; std::vector<uint32_t> indices;
  auto line=[&](size_t a,size_t b) {
    if(a>=p->debugPositions.size() || b>=p->debugPositions.size()) throw std::out_of_range("Debug line index");
    auto v=transformClip(matrix,p->debugPositions[a]),w=transformClip(matrix,p->debugPositions[b]);
    auto dx=(w.x-v.x)*width,dy=(w.y-v.y)*height,length=std::hypot(dx,dy);
    if(length<1e-6f || !std::isfinite(length)) return;
    auto x=-dy/length*p->lineWidth/width,y=dx/length*p->lineWidth/height;
    auto base=uint32_t(vertices.size());
    vertices.insert(vertices.end(),{{v.x+x,v.y+y,v.z},{w.x+x,w.y+y,w.z},{w.x-x,w.y-y,w.z},{v.x-x,v.y-y,v.z}});
    indices.insert(indices.end(),{base,base+1,base+2,base,base+2,base+3});
  };
  if(p->debugIndices.empty()) for(size_t i=0;i+1<p->debugPositions.size();i+=2) line(i,i+1);
  else for(size_t i=0;i+1<p->debugIndices.size();i+=2) line(p->debugIndices[i],p->debugIndices[i+1]);
  drawMesh(0,vertices,{},indices,Mat4::identity(),color);
}
void Backend::generateTextureMipmap(TextureHandle h) { generateTextureMipmap(resource(h)->id); }
void Backend::generateTextureMipmap(uint32_t id) {
  auto& t=p->textures.at(id);
  auto levels=1u+uint32_t(std::floor(std::log2(std::max(t.image.width,t.image.height))));
  if(levels==t.image.mipLevels) { p->c.generateMipmaps(t.image); return; }
  auto pixels=readTexture(id);
  auto w=t.image.width,h=t.image.height;
  p->c.idle();
  if(auto target=p->targets.find(id);target!=p->targets.end()) {
    p->destroyTarget(*target->second); p->targets.erase(target);
  }
  p->c.destroy(t.image);
  t.image=p->c.createImage(w,h,levels);
  p->c.upload(t.image,pixels);
}

void Backend::setDifferenceAggregationEnabled(bool value) { p->differenceEnabled=value; if(!value) p->differenceReady=false; }
bool Backend::isDifferenceAggregationEnabled() { return p->differenceEnabled; }
void Backend::setDifferenceAggregationRegion(const core::DifferenceEvaluationRegion& region) { p->differenceRegion=region; }
core::DifferenceEvaluationRegion Backend::getDifferenceAggregationRegion() { return p->differenceRegion; }
bool Backend::evaluateDifferenceAggregation(Handle id,int w,int h) {
    p->differenceReady=false;
    if(!p->differenceEnabled || !id || w<=0 || h<=0) return false;
    auto region=p->differenceRegion;
    if(region.width<=0 || region.height<=0) region={0,0,w,h};
    region.x=std::clamp(region.x,0,w); region.y=std::clamp(region.y,0,h);
    region.width=std::clamp(region.width,0,w-region.x); region.height=std::clamp(region.height,0,h-region.y);
    if(!region.width || !region.height) return false;
    flush(); p->difference=ngReduceDifference(p->textures.at(uint32_t(id)).image,region);
    return p->differenceReady=true;
}
bool Backend::fetchDifferenceAggregationResult(core::DifferenceEvaluationResult& result) {
    using std::min; using std::max; using std::clamp; using std::floor;
    result={}; if(!p->differenceReady) return false;
    const int width=p->difference.width,height=p->difference.height;
    const auto& data=p->difference.data;
        double totalRed = 0;
        double totalGreen = 0;
        double totalBlue = 0;
        double totalAlpha = 0;

        std::array<double, core::kDiffTileCount> tileTotals{};
        std::array<double, core::kDiffTileCount> tileCounts{};

        double regionWidth = p->difference.sourceWidth > 0 ? static_cast<double>(p->difference.sourceWidth) : static_cast<double>(width);
        double regionHeight = p->difference.sourceHeight > 0 ? static_cast<double>(p->difference.sourceHeight) : static_cast<double>(height);
        if (regionWidth <= 0 || regionHeight <= 0) {
            result = core::DifferenceEvaluationResult{};
            return false;
        }

        double tileWidth = regionWidth / int(core::kDiffTileColumns);
        double tileHeight = regionHeight / int(core::kDiffTileColumns);
        if (tileWidth <= 0 || tileHeight <= 0) {
            result = core::DifferenceEvaluationResult{};
            return false;
        }

        double pixelWidth = regionWidth / static_cast<double>(width);
        double pixelHeight = regionHeight / static_cast<double>(height);
        if (pixelWidth <= 0 || pixelHeight <= 0) {
            result = core::DifferenceEvaluationResult{};
            return false;
        }
        double pixelArea = pixelWidth * pixelHeight;
        if (pixelArea <= 0) {
            result = core::DifferenceEvaluationResult{};
            return false;
        }
        const double epsilon = 1e-9;

        for (int y = 0; y < height; ++y) {
            double srcTop = y * pixelHeight;
            double srcBottom = (y + 1) * pixelHeight;

            for (int x = 0; x < width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x) * 4;

                double r = data[idx + 0];
                double g = data[idx + 1];
                double b = data[idx + 2];
                double a = data[idx + 3];

                totalRed += r;
                totalGreen += g;
                totalBlue += b;
                double weight = a > 0 ? a : 1.0;
                totalAlpha += weight;

                double srcLeft = x * pixelWidth;
                double srcRight = (x + 1) * pixelWidth;

                int tileXStart = static_cast<int>(floor(srcLeft / tileWidth));
                int tileXEnd = static_cast<int>(floor((srcRight - epsilon) / tileWidth));
                int tileYStart = static_cast<int>(floor(srcTop / tileHeight));
                int tileYEnd = static_cast<int>(floor((srcBottom - epsilon) / tileHeight));

                tileXStart = clamp(tileXStart, 0, int(core::kDiffTileColumns) - 1);
                tileXEnd = clamp(tileXEnd, 0, int(core::kDiffTileColumns) - 1);
                tileYStart = clamp(tileYStart, 0, int(core::kDiffTileColumns) - 1);
                tileYEnd = clamp(tileYEnd, 0, int(core::kDiffTileColumns) - 1);

                double sampleTotal = r + g + b;
                for (int tileY = tileYStart; tileY < tileYEnd + 1; ++tileY) {
                    double tileTop = tileY * tileHeight;
                    double tileBottom = tileTop + tileHeight;
                    double overlapY = min(srcBottom, tileBottom) - max(srcTop, tileTop);
                    if (overlapY <= 0) continue;

                    for (int tileX = tileXStart; tileX < tileXEnd + 1; ++tileX) {
                        double tileLeft = tileX * tileWidth;
                        double tileRight = tileLeft + tileWidth;
                        double overlapX = min(srcRight, tileRight) - max(srcLeft, tileLeft);
                        if (overlapX <= 0) continue;

                        double overlapArea = overlapX * overlapY;
                        if (overlapArea <= 0) continue;

                        double fraction = overlapArea / pixelArea;
                        if (fraction <= 0) continue;

                        std::size_t tileIndex = static_cast<std::size_t>(tileY) * int(core::kDiffTileColumns) + tileX;
                        tileTotals[tileIndex] += sampleTotal * fraction;
                        tileCounts[tileIndex] += weight * fraction;
                    }
                }
            }
        }

        if (totalAlpha <= 0) {
            totalAlpha = static_cast<double>(uint32_t(p->difference.sourceWidth*p->difference.sourceHeight));
            for (auto& count : tileCounts) if (count == 0) count = 1.0;
        }

        result.red = totalRed;
        result.green = totalGreen;
        result.blue = totalBlue;
        result.alpha = totalAlpha;
        result.sampleCount = static_cast<std::uint32_t>(totalAlpha);
        result.tileTotals = tileTotals;
        result.tileCounts = tileCounts;

        p->differenceReady = false;
        return true;
}

void Backend::drawTextureAtPart(const core::Texture& texture,const std::shared_ptr<core::nodes::Part>& part) {
    if(!part) return;
    const float w=texture.width()/2,h=texture.height()/2;
    const auto origin=part->core::nodes::Drawable::getMesh().origin;
    const std::array<Vec3,4> positions{{{-w-origin.x,-h-origin.y,0},{w-origin.x,-h-origin.y,0},
        {w-origin.x,h-origin.y,0},{-w-origin.x,h-origin.y,0}}};
    const std::array<Vec2,4> uvs{{{0,0},{1,0},{1,1},{0,1}}};
    const std::array<uint32_t,6> indices{0,1,2,0,2,3};
    const auto matrix=part->currentRenderSpace().matrix*part->immediateModelMatrix();
    drawMesh(texture.backendId(),positions,uvs,indices,matrix,{part->tint.x,part->tint.y,part->tint.z,part->opacity},part->screenTint);
}
void Backend::drawTextureAtPosition(const core::Texture& texture,const Vec2& position,float opacity,const Vec3& color,const Vec3& screen) {
    const auto w=float(texture.width()),h=float(texture.height());
    drawTextureAtRect(texture,{position.x-w*.5f,position.y-h*.5f,w,h},{0,0,1,1},opacity,color,screen,nullptr,nullptr);
}
void Backend::drawTextureAtRect(const core::Texture& texture,const core::Rect& area,const core::Rect& uv,
    float opacity,const Vec3& color,const Vec3& screen,void* shader,void* suppliedCamera) {
    if(shader) throw std::invalid_argument("Use an explicit Metal pipeline for custom texture shaders");
    const auto camera=suppliedCamera?static_cast<core::math::Camera*>(suppliedCamera):core::inGetCameraHandle().get();
    const std::array<Vec3,4> positions{{{area.x,area.y,0},{area.x+area.w,area.y,0},
        {area.x+area.w,area.y+area.h,0},{area.x,area.y+area.h,0}}};
    const std::array<Vec2,4> uvs{{{uv.x,uv.y},{uv.w,uv.y},{uv.w,uv.h},{uv.x,uv.h}}};
    const std::array<uint32_t,6> indices{0,1,2,0,2,3};
    drawMesh(texture.backendId(),positions,uvs,indices,camera->matrix(),{color.x,color.y,color.z,opacity},screen);
}
} // namespace nijigenerate::backend::metal
