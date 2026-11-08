// C++ translation of nijimi/source/render/vulkan/vulkan_backend.d.
// Shader assets and blend/pipeline selection follow that backend; SDL/ImGui
// presentation and native nicxlive packet/resource access are host adaptations.
#include "backend.hpp"
#include "emitter.hpp"
#include "difference.hpp"
#include <nijigenerate/core/dbg.hpp>
#include "vulkan_shaders.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <core/render/backend_queue.hpp>
#include <core/render/shared_deform_buffer.hpp>
#include <core/runtime_state.hpp>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace nijigenerate::backend::vulkan {
namespace core = nicxlive::core;
using namespace nicxlive::core::nodes;
namespace {
constexpr VkImageUsageFlags colorUsage =
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
struct Vertex {
  float x, y, u, v, z = 0;
};
struct Material {
  float tint[4]{1, 1, 1, 1};
  float screen[4]{};
  float emission[4]{}; // x: source is an already-premultiplied composite
};
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
bool portableFixedBlend(BlendMode m) {
  return m == BlendMode::Multiply || m == BlendMode::Screen;
}
VkPipelineColorBlendAttachmentState blendState(BlendMode mode) {
  VkPipelineColorBlendAttachmentState s{};
  s.blendEnable = VK_TRUE;
  s.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  s.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  s.colorBlendOp = VK_BLEND_OP_ADD;
  s.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  s.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  s.alphaBlendOp = VK_BLEND_OP_ADD;

  auto setBlendFunc = [&](VkBlendFactor src, VkBlendFactor dst) {
    s.srcColorBlendFactor = src;
    s.dstColorBlendFactor = dst;
    s.srcAlphaBlendFactor = src;
    s.dstAlphaBlendFactor = dst;
  };
  auto setBlendFuncSeparate =
      [&](VkBlendFactor srcColor, VkBlendFactor dstColor,
          VkBlendFactor srcAlpha, VkBlendFactor dstAlpha) {
        s.srcColorBlendFactor = srcColor;
        s.dstColorBlendFactor = dstColor;
        s.srcAlphaBlendFactor = srcAlpha;
        s.dstAlphaBlendFactor = dstAlpha;
      };

  switch (mode) {
  case BlendMode::Normal:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::Multiply:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_DST_COLOR,
                 VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::Screen:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
    break;
  case BlendMode::Overlay:
  case BlendMode::ColorBurn:
  case BlendMode::HardLight:
  case BlendMode::SoftLight:
  case BlendMode::Difference:
    // Legacy fallback path: approximate unsupported equations.
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::Darken:
    s.colorBlendOp = VK_BLEND_OP_MIN;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE);
    break;
  case BlendMode::Lighten:
    s.colorBlendOp = VK_BLEND_OP_MAX;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE);
    break;
  case BlendMode::ColorDodge:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE);
    break;
  case BlendMode::LinearDodge:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFuncSeparate(
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::AddGlow:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFuncSeparate(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
                         VK_BLEND_FACTOR_ONE,
                         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::Exclusion:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFuncSeparate(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                         VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                         VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE);
    break;
  case BlendMode::Subtract:
    s.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_ONE);
    break;
  case BlendMode::Inverse:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                 VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::DestinationIn:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA);
    break;
  case BlendMode::ClipToLower:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_DST_ALPHA,
                 VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  case BlendMode::SliceFromLower:
    s.colorBlendOp = VK_BLEND_OP_ADD;
    s.alphaBlendOp = VK_BLEND_OP_ADD;
    setBlendFunc(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    break;
  }
  return s;
}

} // namespace
VkBlendOp advancedBlendOpFor(BlendMode mode) {
  switch (mode) {
  case BlendMode::Multiply:
    return VK_BLEND_OP_MULTIPLY_EXT;
  case BlendMode::Screen:
    return VK_BLEND_OP_SCREEN_EXT;
  case BlendMode::Overlay:
    return VK_BLEND_OP_OVERLAY_EXT;
  case BlendMode::Darken:
    return VK_BLEND_OP_DARKEN_EXT;
  case BlendMode::Lighten:
    return VK_BLEND_OP_LIGHTEN_EXT;
  case BlendMode::ColorDodge:
    return VK_BLEND_OP_COLORDODGE_EXT;
  case BlendMode::ColorBurn:
    return VK_BLEND_OP_COLORBURN_EXT;
  case BlendMode::HardLight:
    return VK_BLEND_OP_HARDLIGHT_EXT;
  case BlendMode::SoftLight:
    return VK_BLEND_OP_SOFTLIGHT_EXT;
  case BlendMode::Difference:
    return VK_BLEND_OP_DIFFERENCE_EXT;
  case BlendMode::Exclusion:
    return VK_BLEND_OP_EXCLUSION_EXT;
  case BlendMode::Normal:
  case BlendMode::LinearDodge:
  case BlendMode::AddGlow:
  case BlendMode::Subtract:
  case BlendMode::Inverse:
  case BlendMode::DestinationIn:
  case BlendMode::ClipToLower:
  case BlendMode::SliceFromLower:
    return VK_BLEND_OP_ADD;
  }
  throw std::invalid_argument("Unknown blend mode");
}

struct Backend::Impl {
  Context &c = context();
  struct Texture {
    Image image;
    VkSampler sampler{};
    int channels = 4;
    bool stencil = false;
    bool mipmaps = true;
    bool uploaded = false;
    core::Filtering filtering = core::Filtering::Linear;
    core::Wrapping wrapping = core::Wrapping::Clamp;
    float anisotropy = 1;
  };
  struct Target {
    std::array<uint32_t, 3> colors{};
    // Separate passes keep advanced blending compatible with devices that
    // support only one advanced-blend color attachment. Each pass preserves
    // its own stencil when nested composites interrupt a masked draw.
    std::array<Image, 3> stencil;
    std::array<VkFramebuffer, 3> framebuffer{};
    VkFramebuffer mrtFramebuffer{};
    uint32_t width{}, height{};
  };
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
  bool spoutTarget = false;
  std::map<uint32_t, Texture> textures;
  std::map<core::RenderResourceHandle, std::vector<uint16_t>> indices;
  uint32_t nextTexture = 1, nextIndex = 1;
  std::unique_ptr<Target> root;
  struct Composite {
    std::weak_ptr<core::DynamicCompositeSurface> owner;
    std::unique_ptr<Target> target;
  };
  std::map<const core::DynamicCompositeSurface *, Composite> composites;
  std::map<uint32_t, std::unique_ptr<Target>> targets;
  std::vector<Vec3> debugPositions;
  std::vector<uint16_t> debugIndices;
  float pointSize = 4, lineWidth = 1;
  std::vector<Target *> stack;
  Target *target{};
  std::vector<Vertex> vertices;
  std::vector<uint16_t> elements;
  std::vector<Batch> batches;
  Buffer vertexBuffer, indexBuffer;
  VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
  VkRenderPass pass{}, offscreenPass{};
  VkRenderPass mrtPass{};
  VkDescriptorSetLayout setLayout{};
  VkPipelineLayout layout{};
  VkDescriptorPool pool{};
  std::map<int, VkPipeline> pipelines;
  VkShaderModule vert{}, maskVert{}, frag{}, blendFrag{}, bumpFrag{}, mrtFrag{}, mrtMaskFrag{}, stage1{}, stage2{}, partMask{},
      maskFrag{}, blurFrag{}, emissionFrag{}, lightingFrag{};
  std::map<std::pair<uint32_t,uint32_t>,Image> blendSnapshots;
  uint32_t white{}, black{};
  int mask = 0;
  uint32_t reference = 1;
  bool lighting = false;
  bool postprocessingRequested = false;
  bool scenePending = false;
  bool sceneEnded = false;
  bool differenceEnabled=false,differenceReady=false;
  core::DifferenceEvaluationRegion differenceRegion{};
  DifferenceReduction difference;
  const bool referenceRendering = [] {
    const char* value = std::getenv("NIJIGENERATE_VULKAN_REFERENCE");
    return value && std::strcmp(value, "0") != 0;
  }();

  VkQueryPool profileQueries{};
  bool profilePending = false;
  double timestampPeriod = 0;
  uint64_t timestampMask = ~uint64_t(0);
  struct Profile {
    uint64_t draws{}, passes{}, copyBytes{}, emissionDraws{}, bumpDraws{};
  } profileLast{}, profileSum{};
  double profileGpuMs = 0;
  unsigned profileFrames = 0;

  bool beginProfile(VkCommandBuffer cmd) {
    if (!profileQueries) return false;
    if (profilePending) {
      uint64_t ticks[2]{};
      const auto result = vkGetQueryPoolResults(c.device, profileQueries, 0, 2,
          sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
      if (result == VK_NOT_READY) return false;
      check(result);
      profileGpuMs += double((ticks[1] - ticks[0]) & timestampMask) * timestampPeriod / 1e6;
      profileSum.draws += profileLast.draws; profileSum.passes += profileLast.passes;
      profileSum.copyBytes += profileLast.copyBytes;
      profileSum.emissionDraws += profileLast.emissionDraws;
      profileSum.bumpDraws += profileLast.bumpDraws;
      if (++profileFrames == 120) {
        const double n = profileFrames;
        std::fprintf(stderr, "[vulkan-gpu] backend_ms=%.3f draws=%.1f passes=%.1f copy_MiB=%.2f emission_draws=%.1f bump_draws=%.1f\n",
            profileGpuMs/n, profileSum.draws/n, profileSum.passes/n,
            profileSum.copyBytes/n/1048576., profileSum.emissionDraws/n, profileSum.bumpDraws/n);
        profileGpuMs = 0; profileFrames = 0; profileSum = {};
      }
      profilePending = false;
    }
    vkCmdResetQueryPool(cmd, profileQueries, 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, profileQueries, 0);
    profileLast = {};
    return true;
  }

  bool maskWritten = false;
  bool blurRequested = false;
  std::unique_ptr<Target> blurA, blurB;
  void destroyTarget(Target &t) {
    if (t.mrtFramebuffer) vkDestroyFramebuffer(c.device, t.mrtFramebuffer, nullptr);
    for (auto framebuffer : t.framebuffer)
      if (framebuffer) vkDestroyFramebuffer(c.device, framebuffer, nullptr);
    for (auto &stencil : t.stencil) c.destroy(stencil);
    t = {};
  }
  std::unique_ptr<Target> makeTarget(const std::array<uint32_t, 3> &ids,
                                     bool rootTarget = false) {
    auto t = std::make_unique<Target>();
    t->colors = ids;
    auto &first = textures.at(ids[0]).image;
    t->width = first.width;
    t->height = first.height;
    const unsigned channels = ids[1] && ids[1] != ids[0] ? (ids[2] && ids[2]!=ids[0]?3:2) : 1;
    for (unsigned channel = 0; channel < channels; ++channel) {
      t->stencil[channel] = c.createImage(
          t->width, t->height, stencilFormat,
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
      VkImageView views[2] = {textures.at(ids[channel]).image.attachmentView, t->stencil[channel].attachmentView};
      VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      f.renderPass = rootTarget ? pass : offscreenPass;
      f.attachmentCount = 2;
      f.pAttachments = views;
      f.width = t->width;
      f.height = t->height;
      f.layers = 1;
      check(vkCreateFramebuffer(c.device, &f, nullptr, &t->framebuffer[channel]));
    }
    if (mrtPass && channels == 3) {
      VkImageView views[] = {textures.at(ids[0]).image.attachmentView,
          textures.at(ids[1]).image.attachmentView, textures.at(ids[2]).image.attachmentView,
          t->stencil[0].attachmentView};
      VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      f.renderPass = mrtPass; f.attachmentCount = 4; f.pAttachments = views;
      f.width = t->width; f.height = t->height; f.layers = 1;
      check(vkCreateFramebuffer(c.device, &f, nullptr, &t->mrtFramebuffer));
    }
    return t;
  }
  void initialize() {
    const char* profiling = std::getenv("NIJIGENERATE_VULKAN_PROFILE");
    if (profiling && std::strcmp(profiling, "0") != 0) {
      uint32_t count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(c.physical, &count, nullptr);
      std::vector<VkQueueFamilyProperties> families(count);
      vkGetPhysicalDeviceQueueFamilyProperties(c.physical, &count, families.data());
      const auto bits = families.at(c.family).timestampValidBits;
      if (bits) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(c.physical, &properties);
        timestampPeriod = properties.limits.timestampPeriod;
        timestampMask = bits < 64 ? (uint64_t(1) << bits) - 1 : ~uint64_t(0);
        VkQueryPoolCreateInfo q{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        q.queryType = VK_QUERY_TYPE_TIMESTAMP; q.queryCount = 2;
        check(vkCreateQueryPool(c.device, &q, nullptr, &profileQueries));
      }
    }
    for (auto format : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
      VkFormatProperties properties{};
      vkGetPhysicalDeviceFormatProperties(c.physical, format, &properties);
      if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        stencilFormat = format;
        break;
      }
    }
    if (stencilFormat == VK_FORMAT_UNDEFINED)
      throw std::runtime_error("No supported Vulkan depth/stencil attachment format");
    VkAttachmentDescription attachments[2]{};
    for (int i = 0; i < 2; ++i) {
      auto &a = attachments[i];
      a.format =
          i == 1 ? stencilFormat : VK_FORMAT_R8G8B8A8_UNORM;
      a.samples = VK_SAMPLE_COUNT_1_BIT;
      a.loadOp = a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      a.storeOp = a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      a.initialLayout = a.finalLayout =
          i == 1 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                 : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    VkAttachmentReference
        colors[] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}},
        depth{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = colors;
    sub.pDepthStencilAttachment = &depth;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = dep.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dep.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if(c.advancedBlendSupported) dep.dstAccessMask|=VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
    VkSubpassDependency dependencies[2]{dep,{}};
    auto& blendDependency=dependencies[1];
    blendDependency.srcSubpass=blendDependency.dstSubpass=0;
    blendDependency.srcStageMask=blendDependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    blendDependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    blendDependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
    blendDependency.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 2;
    rp.pAttachments = attachments;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = c.advancedBlendSupported ? 2 : 1;
    rp.pDependencies = dependencies;
    check(vkCreateRenderPass(c.device, &rp, nullptr, &pass));
    check(vkCreateRenderPass(c.device, &rp, nullptr, &offscreenPass));
    // Native advanced blend can be restricted to one color attachment. Keep
    // that path separate; portable blending can use independent MRT states.
    if (c.independentBlendSupported && !c.advancedBlendSupported && !referenceRendering) {
      VkAttachmentDescription mrtAttachments[] = {attachments[0], attachments[0], attachments[0], attachments[1]};
      VkAttachmentReference mrtColors[] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
      VkAttachmentReference mrtDepth{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
      auto mrtSub = sub; mrtSub.colorAttachmentCount = 3; mrtSub.pColorAttachments = mrtColors;
      mrtSub.pDepthStencilAttachment = &mrtDepth;
      auto mrtInfo = rp; mrtInfo.attachmentCount = 4; mrtInfo.pAttachments = mrtAttachments;
      mrtInfo.pSubpasses = &mrtSub;
      check(vkCreateRenderPass(c.device, &mrtInfo, nullptr, &mrtPass));
    }
    VkDescriptorSetLayoutBinding bindings[4]{};
    for (int i = 0; i < 4; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo d{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    d.bindingCount = 4;
    d.pBindings = bindings;
    check(vkCreateDescriptorSetLayout(c.device, &d, nullptr, &setLayout));
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material)};
    VkPipelineLayoutCreateInfo l{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    l.setLayoutCount = 1;
    l.pSetLayouts = &setLayout;
    l.pushConstantRangeCount = 1;
    l.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(c.device, &l, nullptr, &layout));
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32768};
    VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 8192;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &size;
    check(vkCreateDescriptorPool(c.device, &poolInfo, nullptr, &pool));
    auto shader = [&](const auto &data) {
      VkShaderModuleCreateInfo s{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      s.codeSize = sizeof(data);
      s.pCode = data;
      VkShaderModule m;
      check(vkCreateShaderModule(c.device, &s, nullptr, &m));
      return m;
    };
    vert = shader(part_vert);
    maskVert = shader(mask_vert);
    frag = shader(part_frag);
    blendFrag=shader(part_blend_frag); bumpFrag=shader(part_bump_frag);
    mrtFrag=shader(part_mrt_frag);
    mrtMaskFrag=shader(part_mrt_mask_frag);
    stage1 = shader(part_stage1_frag);
    stage2 = shader(part_stage2_frag);
    partMask = shader(part_mask_frag);
    maskFrag = shader(mask_frag);
    blurFrag = shader(overlay_blur_frag);
    emissionFrag = shader(emission_combine_frag);
    lightingFrag=shader(lighting_frag);
  }
  VkPipeline pipeline(const Batch &b, unsigned channel) {
    bool offscreen = b.target != root.get();
    const bool mrt = channel == 3;
    const bool portableAdvanced = !c.advancedBlendSupported && advanced(b.blend) && !b.fixedBlend;
    const int equation = b.fixedBlend ? (b.blend == BlendMode::Multiply ? 1 : 2) : portableAdvanced ? 3 : 0;
    int key = int(b.blend) * 256 + b.kind + (offscreen ? 16 : 0) + int(channel) * 32 +
        (b.depth ? 65536 : 0) + (b.fixedBlend ? 131072 : 0);
    if (auto it = pipelines.find(key); it != pipelines.end())
      return it->second;
    VkPipelineShaderStageCreateInfo stages[2]{};
    VkSpecializationMapEntry entry{0, 0, sizeof(equation)};
    VkSpecializationInfo specialization{1, &entry, sizeof(equation), &equation};
    for (int i = 0; i < 2; ++i) {
      stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[i].stage =
          i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[i].module = i == 0
                             ? ((b.kind == 4 || b.kind == 8) ? maskVert : vert)
                             : (mrt && b.kind == 3 ? mrtMaskFrag
                                : mrt && b.mask != 1 ? mrtFrag
                                : channel == 2 && b.mask != 1 && b.kind != 3 ? bumpFrag
                                : channel == 0 && portableAdvanced ? blendFrag
                                : b.kind == 13 ? lightingFrag
                                : b.kind == 11                   ? emissionFrag
                                : channel == 1 && b.mask != 1 && b.kind != 3 ? stage2
                                : b.kind == 1                 ? stage1
                                : b.kind == 2                  ? stage2
                                : b.kind == 9                  ? blurFrag
                                : (b.kind == 3 || b.kind == 6 || b.kind == 7) ? partMask
                                : (b.kind == 4 || b.kind == 8) ? maskFrag
                                                               : frag);
      stages[i].pName = "main";
      if (i == 1 && (stages[i].module == mrtFrag || stages[i].module == frag))
        stages[i].pSpecializationInfo = &specialization;
    }
    VkVertexInputBindingDescription binding{0, sizeof(Vertex),
                                            VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
        {2, 0, VK_FORMAT_R32_SFLOAT, 16}};
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = ds.depthWriteEnable = b.depth;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    ds.stencilTestEnable = b.mask != 0;
    ds.front.compareOp =
        b.mask == 1 ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_EQUAL;
    ds.front.passOp = b.mask == 1 ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP;
    ds.front.failOp = ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.front.compareMask = 255;
    ds.front.writeMask = b.mask == 1 ? 255 : 0;
    ds.back = ds.front;
    std::array<VkPipelineColorBlendAttachmentState, 3> blends;
    blends.fill(blendState(b.blend));
    auto& blend = blends[0];
    bool useAdvanced = channel == 0 && c.advancedBlendSupported && advanced(b.blend);
    VkPipelineColorBlendAdvancedStateCreateInfoEXT advancedInfo{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT};
    if (useAdvanced) {
      blend.colorBlendOp = blend.alphaBlendOp = advancedBlendOpFor(b.blend);
      blend.srcColorBlendFactor = blend.dstColorBlendFactor =
          blend.srcAlphaBlendFactor = blend.dstAlphaBlendFactor =
              VK_BLEND_FACTOR_ONE;
      advancedInfo.srcPremultiplied = advancedInfo.dstPremultiplied = VK_TRUE;
      advancedInfo.blendOverlap = VK_BLEND_OVERLAP_UNCORRELATED_EXT;
    }
    if ((channel == 0 || mrt) && b.fixedBlend) {
      blend.srcColorBlendFactor = b.blend == BlendMode::Multiply ? VK_BLEND_FACTOR_DST_COLOR : VK_BLEND_FACTOR_ONE;
      blend.dstColorBlendFactor = b.blend == BlendMode::Multiply ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    if ((channel == 0 || mrt) && portableAdvanced) blend.blendEnable=VK_FALSE;
    for (auto& attachment : blends) attachment.colorWriteMask = b.mask == 1 ? 0 : 15;
    blend.colorWriteMask = b.mask == 1 ? 0 : 15;
    if (b.kind == 9 || b.kind == 13)
      blend.blendEnable = VK_FALSE;
    if (b.kind == 10) {
      blend.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
      blend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    if (b.kind == 11) {
      blend.srcColorBlendFactor = blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = mrt ? 3 : 1;
    cb.pAttachments = blends.data();
    if (useAdvanced)
      cb.pNext = &advancedInfo;
    VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                VK_DYNAMIC_STATE_SCISSOR,
                                VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dy{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 3;
    dy.pDynamicStates = dynamic;
    VkGraphicsPipelineCreateInfo info{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState = &vp;
    info.pRasterizationState = &rs;
    info.pMultisampleState = &ms;
    info.pDepthStencilState = &ds;
    info.pColorBlendState = &cb;
    info.pDynamicState = &dy;
    info.layout = layout;
    info.renderPass = mrt ? mrtPass : offscreen ? offscreenPass : pass;
    VkPipeline result;
    check(vkCreateGraphicsPipelines(c.device, VK_NULL_HANDLE, 1, &info, nullptr,
                                    &result));
    pipelines.emplace(key, result);
    return result;
  }
  bool hasOverlappingTriangles(const Batch& b) const {
    struct Triangle {
      std::array<Vertex, 3> p;
      float minX, maxX, minY, maxY;
    };
    std::vector<Triangle> triangles;
    triangles.reserve(b.count / 3);
    for (uint32_t i = 0; i + 2 < b.count; i += 3) {
      Triangle t;
      for (unsigned j = 0; j < 3; ++j) {
        t.p[j] = vertices.at(size_t(b.base) + elements.at(b.first + i + j));
        if (!std::isfinite(t.p[j].x) || !std::isfinite(t.p[j].y)) return true;
      }
      t.minX = std::min({t.p[0].x,t.p[1].x,t.p[2].x});
      t.maxX = std::max({t.p[0].x,t.p[1].x,t.p[2].x});
      t.minY = std::min({t.p[0].y,t.p[1].y,t.p[2].y});
      t.maxY = std::max({t.p[0].y,t.p[1].y,t.p[2].y});
      const double area = (double(t.p[1].x)-t.p[0].x)*(double(t.p[2].y)-t.p[0].y) -
          (double(t.p[1].y)-t.p[0].y)*(double(t.p[2].x)-t.p[0].x);
      if (area != 0) triangles.push_back(t);
    }
    std::sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b) { return a.minX < b.minX; });
    const auto separated = [](const Triangle& a,const Triangle& b,const Triangle& axes) {
      for (unsigned i = 0; i < 3; ++i) {
        const auto& p = axes.p[i]; const auto& q = axes.p[(i+1)%3];
        const double x = double(q.y)-p.y, y = double(p.x)-q.x;
        double loA = INFINITY, hiA = -INFINITY, loB = INFINITY, hiB = -INFINITY;
        for (unsigned j = 0; j < 3; ++j) {
          const double u = a.p[j].x*x + a.p[j].y*y, v = b.p[j].x*x + b.p[j].y*y;
          loA = std::min(loA,u); hiA = std::max(hiA,u);
          loB = std::min(loB,v); hiB = std::max(hiB,v);
        }
        // Shared mesh edges do not overlap under the rasterizer's top-left rule.
        if (hiA <= loB + 1e-14 || hiB <= loA + 1e-14) return true;
      }
      return false;
    };
    for (size_t i = 0; i < triangles.size(); ++i) {
      const auto& a = triangles[i];
      for (size_t j = i+1; j < triangles.size() && triangles[j].minX < a.maxX; ++j) {
        const auto& b = triangles[j];
        if (b.maxY <= a.minY || b.minY >= a.maxY) continue;
        if (!separated(a,b,a) && !separated(a,b,b)) return true;
      }
    }
    return false;
  }
  VkRect2D pixelBounds(const Batch& b) const {
    const auto& t = *b.target;
    double minX = t.width, minY = t.height, maxX = 0, maxY = 0;
    for (uint32_t i = 0; i < b.count; ++i) {
      const auto& v = vertices.at(size_t(b.base) + elements.at(b.first+i));
      if (!std::isfinite(v.x) || !std::isfinite(v.y)) return {{0,0},{t.width,t.height}};
      const double x = (double(v.x)*.5+.5)*t.width;
      const double y = (&t == root.get() ? .5-double(v.y)*.5 : .5+double(v.y)*.5)*t.height;
      minX = std::min(minX,x); maxX = std::max(maxX,x);
      minY = std::min(minY,y); maxY = std::max(maxY,y);
    }
    // Include a pixel around the geometry for viewport rounding. The portable
    // shader fetches the background only at gl_FragCoord, without a filter.
    const auto x0 = int32_t(std::clamp(std::floor(minX)-1,0.,double(t.width)));
    const auto y0 = int32_t(std::clamp(std::floor(minY)-1,0.,double(t.height)));
    const auto x1 = int32_t(std::clamp(std::ceil(maxX)+1,0.,double(t.width)));
    const auto y1 = int32_t(std::clamp(std::ceil(maxY)+1,0.,double(t.height)));
    return {{x0,y0},{uint32_t(std::max(0,x1-x0)),uint32_t(std::max(0,y1-y0))}};
  }
  void record() {
    auto cmd = c.command;
    const bool profiling = beginProfile(cmd);
    check(vkResetDescriptorPool(c.device, pool, 0));
    c.ensureBuffer(vertexBuffer, vertices.size() * sizeof(Vertex),
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    c.ensureBuffer(indexBuffer, elements.size() * sizeof(uint16_t),
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if (!vertices.empty())
      std::memcpy(vertexBuffer.mapped, vertices.data(),
                  vertices.size() * sizeof(Vertex));
    if (!elements.empty())
      std::memcpy(indexBuffer.mapped, elements.data(),
                  elements.size() * sizeof(uint16_t));
    Target *active = nullptr;
    unsigned activeChannel = 0;
    auto end = [&] {
      if (active) {
        vkCmdEndRenderPass(cmd);
        for (unsigned i = 0; i < (activeChannel == 3 ? 3u : 1u); ++i)
          c.transition(cmd, textures.at(active->colors[activeChannel == 3 ? i : activeChannel]).image,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        active = nullptr;
      }
    };
    // Finish both color planes before a target is sampled by its parent.
    // Group consecutive draws to avoid changing render passes for every part.
    std::vector<std::pair<const Batch *, unsigned>> draws;
    draws.reserve(batches.size() * 2);
    // Both planes sample the same inputs. Reuse their descriptor set so
    // separating emission does not halve the supported number of part draws.
    std::vector<VkDescriptorSet> descriptorSets(batches.size()*4);
    for (auto& b : batches) {
      // Fixed blending accumulates overlapping triangles, while the portable
      // shader samples one pre-Part snapshot. Preserve the latter for folds.
      b.fixedBlend = !referenceRendering && !c.advancedBlendSupported && b.count &&
          (b.kind == 0 || b.kind == 5) && b.mask != 1 && portableFixedBlend(b.blend) &&
          !hasOverlappingTriangles(b);
    }
    for (size_t first = 0; first < batches.size();) {
      size_t last = first + 1;
      const bool sceneDraw = batches[first].kind < 9;
      while (last < batches.size() && batches[last].target == batches[first].target &&
             (batches[last].kind < 9) == sceneDraw) ++last;
      const bool mrt = sceneDraw && batches[first].target->mrtFramebuffer;
      const unsigned channels = sceneDraw && batches[first].target->framebuffer[1] ? (batches[first].target->framebuffer[2]?3:2) : 1;
      if (mrt) {
        for (size_t i = first; i < last; ++i) draws.emplace_back(&batches[i], 3);
      } else for (unsigned channel = 0; channel < channels; ++channel)
          for (size_t i = first; i < last; ++i) draws.emplace_back(&batches[i], channel);
      first = last;
    }
    for (const auto &[batch, channel] : draws) {
      const auto &b = *batch;
      auto &t = *b.target;
      if(b.kind==14) {
        end();
        for(auto id:t.colors) if(id) c.generateMipmaps(cmd,textures.at(id).image);
        continue;
      }
      Image* snapshot=nullptr;
      if((channel==0 || channel==3) && b.count && ((!c.advancedBlendSupported && advanced(b.blend) && !b.fixedBlend) || b.kind==13)) {
        end();
        if(b.kind==13) c.generateMipmaps(cmd,textures.at(t.colors[1]).image);
        auto& image=blendSnapshots[{t.width,t.height}];
        if(!image.handle) image=c.createImage(t.width,t.height,VK_FORMAT_R8G8B8A8_UNORM,colorUsage);
        auto& source=textures.at(t.colors[0]).image;
        c.transition(cmd,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        c.transition(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy copy{}; copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        const auto region = !referenceRendering && b.kind != 13 ? pixelBounds(b) : VkRect2D{{0,0},{t.width,t.height}};
        copy.srcOffset=copy.dstOffset={region.offset.x,region.offset.y,0};
        copy.extent={region.extent.width,region.extent.height,1};
        if (region.extent.width && region.extent.height)
          vkCmdCopyImage(cmd,source.handle,source.layout,image.handle,image.layout,1,&copy);
        if (profiling) profileLast.copyBytes += uint64_t(region.extent.width)*region.extent.height*4;
        c.transition(cmd,image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); snapshot=&image;
      }
      if (active != &t || activeChannel != channel) {
        end();
        for (unsigned i = 0; i < (channel == 3 ? 3u : 1u); ++i)
          c.transition(cmd, textures.at(t.colors[channel == 3 ? i : channel]).image,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const unsigned stencilIndex = channel == 3 ? 0 : channel;
        if (t.stencil[stencilIndex].handle)
          c.transition(cmd, t.stencil[stencilIndex],
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        r.renderPass = channel == 3 ? mrtPass : &t == root.get() ? pass : offscreenPass;
        r.framebuffer = channel == 3 ? t.mrtFramebuffer : t.framebuffer[channel];
        r.renderArea.extent = {t.width, t.height};
        vkCmdBeginRenderPass(cmd, &r, VK_SUBPASS_CONTENTS_INLINE);
        if (profiling) ++profileLast.passes;
        active = &t;
        activeChannel = channel;
        bool offscreen = &t != root.get();
        VkViewport vp{0,
                      offscreen ? 0.f : float(t.height),
                      float(t.width),
                      offscreen ? float(t.height) : -float(t.height),
                      0,
                      1};
        VkRect2D sc{{0, 0}, {t.width, t.height}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);
      }
      if (b.clear || b.clearStencil >= 0) {
        std::vector<VkClearAttachment> clears;
        if (b.clear) {
          for (uint32_t i = 0; i < (channel == 3 ? 3u : 1u); ++i) {
            VkClearAttachment a{};
            a.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            a.colorAttachment = i;
            a.clearValue.color = {{0, 0, 0, 0}};
            if ((channel == 0 || (channel == 3 && i == 0)) && &t == root.get())
              core::inGetClearColor(
                  a.clearValue.color.float32[0], a.clearValue.color.float32[1],
                  a.clearValue.color.float32[2], a.clearValue.color.float32[3]);
            if (b.customClear) a.clearValue.color = {{b.clearColor.x,b.clearColor.y,b.clearColor.z,b.clearColor.w}};
            clears.push_back(a);
          }
        }
        VkClearAttachment stencil{};
        stencil.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT | (b.clear ? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
        stencil.clearValue.depthStencil = {
            1, uint32_t(std::max(0, b.clearStencil))};
        if (t.stencil[channel == 3 ? 0 : channel].handle)
          clears.push_back(stencil);
        VkClearRect rect{{{0, 0}, {t.width, t.height}}, 0, 1};
        vkCmdClearAttachments(cmd, uint32_t(clears.size()), clears.data(), 1,
                              &rect);
      }
      if (!b.count)
        continue;
      auto &set = descriptorSets[size_t(batch - batches.data())*4+channel];
      if (!set) {
        VkDescriptorSetAllocateInfo ai{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &setLayout;
        check(vkAllocateDescriptorSets(c.device, &ai, &set));
        VkDescriptorImageInfo images[4]{};
        VkWriteDescriptorSet writes[4]{};
        for (int i = 0; i < 4; ++i) {
          auto id = i<3?b.textures[i]:black;
          if (!id || !textures.contains(id))
            id = i == 0 ? white : black;
          auto &tex = textures.at(id);
          images[i] = {tex.sampler ? tex.sampler : c.sampler, tex.image.view,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          if(((i==3 && b.kind!=13) || (i==0 && b.kind==13)) && snapshot) images[i]={c.sampler,snapshot->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
          writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes[i].dstSet = set;
          writes[i].dstBinding = i;
          writes[i].descriptorCount = 1;
          writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          writes[i].pImageInfo = &images[i];
        }
        vkUpdateDescriptorSets(c.device, 4, writes, 0, nullptr);
      }
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline(b, channel));
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                              1, &set, 0, nullptr);
      auto material=b.material; material.emission[1]=float(b.blend);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(Material), &material);
      vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK,
                               b.reference);
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.handle, &offset);
      vkCmdBindIndexBuffer(cmd, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
      if (channel == 0 && c.advancedBlendSupported && !c.advancedBlendCoherent && advanced(b.blend)) {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
        // Noncoherent hardware also needs ordering for overlapping triangles
        // within a part. Coherent devices keep the single-draw fast path.
        for(uint32_t offset=0;offset<b.count;offset+=3) {
          vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_DEPENDENCY_BY_REGION_BIT,
              1,&barrier,0,nullptr,0,nullptr);
          vkCmdDrawIndexed(cmd,std::min(3u,b.count-offset),1,b.first+offset,b.base,0);
          if (profiling) ++profileLast.draws;
        }
      } else {
        vkCmdDrawIndexed(cmd, b.count, 1, b.first, b.base, 0);
        if (profiling) ++profileLast.draws;
      }
      if (profiling && channel == 1) ++profileLast.emissionDraws;
      if (profiling && channel == 2) ++profileLast.bumpDraws;
    }
    end();
    if (profiling) {
      vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, profileQueries, 1);
      profilePending = true;
    }
  }
};
Backend &backend() {
  auto b = std::dynamic_pointer_cast<Backend>(core::getCurrentRenderBackend());
  if (!b)
    throw std::logic_error("Vulkan model backend is not active");
  return *b;
}
ImTextureID uiTexture(uint32_t id) { return id ? backend().uiTexture(id) : 0; }
Backend::Backend() : p(std::make_unique<Impl>()) { p->c.renderBackend_ = this; }
Backend::~Backend() { shutdown(); }
void Backend::shutdown() {
  if (!p)
    return;
  p->c.idle();
  if (p->profileQueries) vkDestroyQueryPool(p->c.device, p->profileQueries, nullptr);
  if (p->root)
    p->destroyTarget(*p->root);
  if (p->blurA)
    p->destroyTarget(*p->blurA);
  if (p->blurB)
    p->destroyTarget(*p->blurB);
  for (auto &[_, v] : p->composites) p->destroyTarget(*v.target);
  for (auto &[_, v] : p->targets) p->destroyTarget(*v);
  for (auto &[_, v] : p->textures) {
    if (v.sampler)
      vkDestroySampler(p->c.device, v.sampler, nullptr);
    p->c.destroy(v.image);
  }
  p->c.destroy(p->vertexBuffer);
  p->c.destroy(p->indexBuffer);
  for (auto &[_, v] : p->pipelines)
    vkDestroyPipeline(p->c.device, v, nullptr);
  vkDestroyShaderModule(p->c.device, p->vert, nullptr);
  vkDestroyShaderModule(p->c.device, p->frag, nullptr);
  vkDestroyShaderModule(p->c.device,p->blendFrag,nullptr); vkDestroyShaderModule(p->c.device,p->bumpFrag,nullptr);
  vkDestroyShaderModule(p->c.device,p->mrtFrag,nullptr);
  vkDestroyShaderModule(p->c.device,p->mrtMaskFrag,nullptr);
  for(auto& [_,image]:p->blendSnapshots) p->c.destroy(image);
  for (auto shader : {p->maskVert, p->stage1, p->stage2, p->partMask,
                      p->maskFrag, p->blurFrag, p->emissionFrag, p->lightingFrag})
    vkDestroyShaderModule(p->c.device, shader, nullptr);
  vkDestroyDescriptorPool(p->c.device, p->pool, nullptr);
  vkDestroyPipelineLayout(p->c.device, p->layout, nullptr);
  vkDestroyDescriptorSetLayout(p->c.device, p->setLayout, nullptr);
  vkDestroyRenderPass(p->c.device, p->pass, nullptr);
  vkDestroyRenderPass(p->c.device, p->offscreenPass, nullptr);
  if (p->mrtPass) vkDestroyRenderPass(p->c.device, p->mrtPass, nullptr);
  p->c.renderBackend_ = nullptr;
  p.reset();
}
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
  t.image = p->c.createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM, colorUsage);
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
    t.image = p->c.createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM, colorUsage);
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
  auto &t = p->textures.at(id);
  t.filtering = filtering; t.wrapping = wrapping; t.anisotropy=std::clamp(anisotropy,1.f,p->c.maxAnisotropy);
  p->c.idle();
  if (t.sampler)
    vkDestroySampler(p->c.device, t.sampler, nullptr);
  VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  s.magFilter = s.minFilter = filtering == core::Filtering::Linear
                                  ? VK_FILTER_LINEAR
                                  : VK_FILTER_NEAREST;
  s.mipmapMode = filtering==core::Filtering::Linear?VK_SAMPLER_MIPMAP_MODE_LINEAR:VK_SAMPLER_MIPMAP_MODE_NEAREST;
  s.maxLod=t.mipmaps?VK_LOD_CLAMP_NONE:0;
  s.anisotropyEnable=p->c.maxAnisotropy>1 && t.anisotropy>1;
  s.maxAnisotropy=t.anisotropy;
  s.addressModeU = s.addressModeV = s.addressModeW =
      wrapping == core::Wrapping::Clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : wrapping == core::Wrapping::Mirror ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT;
  check(vkCreateSampler(p->c.device, &s, nullptr, &t.sampler));
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
  if (it->second.sampler)
    vkDestroySampler(p->c.device, it->second.sampler, nullptr);
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
    throw std::runtime_error("Missing Vulkan drawable indices");
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
void Backend::issueBlendBarrier() {} // Recorded at each dependent Vulkan attachment draw.
bool Backend::hasTexture(uint32_t id) const { return p && p->textures.contains(id); }
void Backend::flush() {
  if (!p || (p->batches.empty() && !(p->scenePending && p->sceneEnded))) return;
  if(p->c.command) { render(); return; }
  p->c.idle();
  p->c.immediate([&](VkCommandBuffer cmd) {
    p->c.command=cmd;
    try { render(); } catch(...) { p->c.command=VK_NULL_HANDLE; throw; }
    p->c.command=VK_NULL_HANDLE;
  });
}

void Backend::pushTarget(uint32_t id,Vec4 color,bool clear) {
  auto& target=p->targets[id];
  if(!target) target=p->makeTarget({id,id,id});
  p->stack.push_back(p->target); p->target=target.get();
  Impl::Batch b; b.target=p->target; b.clear=clear; b.customClear=true; b.clearColor=color; p->batches.push_back(b);
}
void Backend::popTarget() {
  if(p->stack.empty()) throw std::logic_error("Unbalanced Vulkan target scope");
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
  t.image=p->c.createImage(w,h,VK_FORMAT_R8G8B8A8_UNORM,colorUsage,VK_IMAGE_ASPECT_COLOR_BIT,levels);
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
    if(shader) throw std::invalid_argument("Use an explicit Vulkan pipeline for custom texture shaders");
    const auto camera=suppliedCamera?static_cast<core::math::Camera*>(suppliedCamera):core::inGetCameraHandle().get();
    const std::array<Vec3,4> positions{{{area.x,area.y,0},{area.x+area.w,area.y,0},
        {area.x+area.w,area.y+area.h,0},{area.x,area.y+area.h,0}}};
    const std::array<Vec2,4> uvs{{{uv.x,uv.y},{uv.w,uv.y},{uv.w,uv.h},{uv.x,uv.h}}};
    const std::array<uint32_t,6> indices{0,1,2,0,2,3};
    drawMesh(texture.backendId(),positions,uvs,indices,camera->matrix(),{color.x,color.y,color.z,opacity},screen);
}
} // namespace nijigenerate::backend::vulkan
