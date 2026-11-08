// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "difference.hpp"
#include "compute_internal.hpp"
#include "metal_shaders.hpp"
#include <array>
#include <cstring>
namespace nijigenerate::backend::metal {
DifferenceReduction ngReduceDifference(Image& source,nicxlive::core::DifferenceEvaluationRegion region) {
    @autoreleasepool {
        auto& c=context(); c.idle();
        if(c.recording()) throw std::logic_error("Difference readback requires a completed frame");
        auto pipeline=ngComputePipeline("difference",difference_source);
        const auto size=size_t(region.width)*region.height*4*sizeof(float);
        if(size>c.p->device.maxBufferLength) throw std::length_error("Difference buffer exceeds Metal device limits");
        std::array<id<MTLBuffer>,2> buffers;
        for(auto& buffer:buffers) {
            buffer=[c.p->device newBufferWithLength:size options:MTLResourceStorageModeShared];
            if(!buffer) throw std::runtime_error("Metal difference allocation failed");
        }
        DifferenceReduction result; result.sourceWidth=region.width; result.sourceHeight=region.height;
        int last=0;
        c.immediate([&] {
            int w=region.width,h=region.height,stage=0;
            do {
                int nw=(w>16 || h>16)?(w+1)/2:w,nh=(w>16 || h>16)?(h+1)/2:h;
                int32_t params[]{w,h,nw,nh,region.x,region.y,stage,int32_t(source.height)};
                auto encoder=[c.p->command computeCommandEncoder]; [encoder setComputePipelineState:pipeline];
                [encoder setTexture:native(source) atIndex:0];
                [encoder setBuffer:buffers[1-stage%2] offset:0 atIndex:1];
                [encoder setBuffer:buffers[stage%2] offset:0 atIndex:2];
                [encoder setBytes:params length:sizeof(params) atIndex:3];
                [encoder dispatchThreadgroups:MTLSizeMake((nw+7)/8,(nh+7)/8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
                [encoder endEncoding];
                last=stage%2; ++stage; w=nw; h=nh;
            } while(w>16 || h>16);
            result.width=w; result.height=h;
        });
        result.data.resize(size_t(result.width)*result.height*4);
        std::memcpy(result.data.data(),buffers[last].contents,result.data.size()*sizeof(float)); return result;
    }
}
}
