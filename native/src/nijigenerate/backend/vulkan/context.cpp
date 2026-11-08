#include "context.hpp"
#include "backend.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace nijigenerate::backend::vulkan {
namespace {
Context *current{};

using FrameClock = std::chrono::steady_clock;

struct FrameProfile {
  FrameClock::time_point previousEnd{}, begin{}, ready{}, present{}, submitted{};
  double updateMs{}, waitMs{}, recordMs{}, submitMs{}, presentMs{};
  unsigned frames{};
};

FrameProfile frameProfile;

double elapsedMs(FrameClock::time_point start, FrameClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

bool enabled(const char *name) {
  const char *value = std::getenv(name);
  return value && std::strcmp(value, "1") == 0;
}

}
void check(VkResult result, const std::source_location &location) {
  if (result < 0)
    throw std::runtime_error("Vulkan error " + std::to_string(result) + " at " +
                             location.file_name() + ":" + std::to_string(location.line()));
}
Context &context() {
  if (!current)
    throw std::logic_error("Vulkan context is not initialized");
  return *current;
}
Context::Context(SDL_Window *window) : window_(window) {
  if (current)
    throw std::logic_error("Only one application Vulkan context is supported");
  try {
    uint32_t count = 0;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions)
      throw std::runtime_error(SDL_GetError());
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "nijigenerate";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    std::vector<const char *> instanceExtensions(extensions,
                                                 extensions + count);
#ifdef __APPLE__
    instanceExtensions.push_back("VK_KHR_portability_enumeration");
    ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    ci.enabledExtensionCount = uint32_t(instanceExtensions.size());
    ci.ppEnabledExtensionNames = instanceExtensions.data();
    check(vkCreateInstance(&ci, nullptr, &instance));
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &swap_.Surface))
      throw std::runtime_error(SDL_GetError());
    check(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    // A graphics family must also be able to present to this actual SDL
    // surface.
    for (auto candidate : devices) {
      uint32_t n = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
      std::vector<VkQueueFamilyProperties> props(n);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, props.data());
      for (uint32_t i = 0; i < n; ++i) {
        VkBool32 present = 0;
        check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, swap_.Surface,
                                                   &present));
        if (present && ((props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))) {
          physical = candidate;
          family = i;
          break;
        }
      }
      if (physical)
        break;
    }
    if (!physical)
      throw std::runtime_error("No Vulkan graphics/present device");
    float priority = 1;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = family;
    q.queueCount = 1;
    q.pQueuePriorities = &priority;
    uint32_t extensionCount = 0;
    check(vkEnumerateDeviceExtensionProperties(physical, nullptr,
                                               &extensionCount, nullptr));
    std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
    check(vkEnumerateDeviceExtensionProperties(
        physical, nullptr, &extensionCount, deviceExtensions.data()));
    std::vector<const char *> required{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const auto &e : deviceExtensions)
      if (!enabled("NIJIGENERATE_VULKAN_PORTABLE_BLEND") &&
          std::strcmp(e.extensionName,
                      VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME) == 0)
        advancedBlendSupported = true;
    if (advancedBlendSupported)
      required.push_back(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME);
    for (const auto &e : deviceExtensions)
      if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0)
        required.push_back("VK_KHR_portability_subset");
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceFeatures supported{}; vkGetPhysicalDeviceFeatures(physical,&supported);
    independentBlendSupported = supported.independentBlend == VK_TRUE;
    features.independentBlend = supported.independentBlend;
    features.samplerAnisotropy=supported.samplerAnisotropy;
    if(features.samplerAnisotropy) {
      VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical,&properties);
      maxAnisotropy=properties.limits.maxSamplerAnisotropy;
    }
    VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT blendFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT};
    if (advancedBlendSupported) {
      VkPhysicalDeviceFeatures2 available{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      available.pNext=&blendFeatures;
      vkGetPhysicalDeviceFeatures2(physical,&available);
      advancedBlendCoherent=blendFeatures.advancedBlendCoherentOperations==VK_TRUE;
    }
    VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    if (advancedBlendSupported) dc.pNext=&blendFeatures;
    dc.queueCreateInfoCount = 1;
    dc.pQueueCreateInfos = &q;
    dc.enabledExtensionCount = uint32_t(required.size());
    dc.ppEnabledExtensionNames = required.data();
    dc.pEnabledFeatures = &features;
    check(vkCreateDevice(physical, &dc, nullptr, &device));
    vkGetDeviceQueue(device, family, 0, &queue);
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16384},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 128}};
    VkDescriptorPoolCreateInfo dp{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dp.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dp.maxSets = 8192;
    dp.poolSizeCount = 3;
    dp.pPoolSizes = sizes;
    check(vkCreateDescriptorPool(device, &dp, nullptr, &descriptors));
    VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sc.magFilter = sc.minFilter = VK_FILTER_LINEAR;
    sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sc.addressModeU = sc.addressModeV = sc.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sc.maxLod = VK_LOD_CLAMP_NONE;
    check(vkCreateSampler(device, &sc, nullptr, &sampler));
    VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pc.queueFamilyIndex = family;
    pc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check(vkCreateCommandPool(device, &pc, nullptr, &uploadPool_));
    VkFormat formats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    swap_.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        physical, swap_.Surface, formats, 2, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    swap_.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    swap_.ClearEnable = true;
    resize();
    current = this;
  } catch (...) {
    release();
    throw;
  }
}
Context::~Context() { release(); }
void Context::release() {
  if (device)
    vkDeviceWaitIdle(device);
  if (renderBackend_)
    renderBackend_->shutdown();
  shutdownImGui();
  if (device) {
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyCommandPool(device, uploadPool_, nullptr);
    vkDestroyDescriptorPool(device, descriptors, nullptr);
    // The ImGui helper owns and destroys the surface together with the
    // swapchain. Do not restore its freed handle for a second destruction.
    ImGui_ImplVulkanH_DestroyWindow(instance, device, &swap_, nullptr);
    vkDestroyDevice(device, nullptr);
  }
  if (instance && swap_.Surface)
    vkDestroySurfaceKHR(instance, swap_.Surface, nullptr);
  if (instance)
    vkDestroyInstance(instance, nullptr);
  if (current == this)
    current = nullptr;
}
void Context::waitForPreviousFrame() {
  if (lastSubmit_) {
    check(vkWaitForFences(device, 1, &lastSubmit_, VK_TRUE, UINT64_MAX));
    lastSubmit_ = VK_NULL_HANDLE;
  }
}
void Context::idle() {
  check(vkDeviceWaitIdle(device));
  lastSubmit_ = VK_NULL_HANDLE;
}
void Context::resize() {
  int w, h;
  SDL_GetWindowSizeInPixels(window_, &w, &h);
  if (w <= 0 || h <= 0) {
    rebuild_ = true;
    return;
  }
  idle();
  VkPresentModeKHR modes[] = {throttle_ > 0 ? (enabled("NIJIGENERATE_VULKAN_MAILBOX")
                                                   ? VK_PRESENT_MODE_MAILBOX_KHR
                                                   : VK_PRESENT_MODE_FIFO_KHR)
                                            : VK_PRESENT_MODE_IMMEDIATE_KHR,
                              VK_PRESENT_MODE_FIFO_KHR};
  swap_.PresentMode =
      ImGui_ImplVulkanH_SelectPresentMode(physical, swap_.Surface, modes, 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(
      instance, physical, device, &swap_, family, nullptr, w, h, 2);
  rebuild_ = false;
}
void Context::setThrottle(int rate) {
  if ((rate > 0) != (throttle_ > 0))
    rebuild_ = true;
  throttle_ = rate;
}
void Context::initializeImGui() {
  ImGui_ImplVulkan_InitInfo i{};
  i.ApiVersion = VK_API_VERSION_1_1;
  i.Instance = instance;
  i.PhysicalDevice = physical;
  i.Device = device;
  i.QueueFamily = family;
  i.Queue = queue;
  i.DescriptorPool = descriptors;
  i.MinImageCount = 2;
  i.ImageCount = swap_.ImageCount;
  i.RenderPass = swap_.RenderPass;
  i.CheckVkResultFn = [](VkResult result) { check(result); };
  imgui_ = ImGui_ImplVulkan_Init(&i);
  if (!imgui_)
    throw std::runtime_error("ImGui Vulkan initialization failed");
}
void Context::shutdownImGui() {
  if (imgui_) {
    idle();
    ImGui_ImplVulkan_Shutdown();
    imgui_ = false;
  }
}
bool Context::beginFrame() {
  static const bool profile = enabled("NIJIGENERATE_VULKAN_PROFILE");
  if (profile)
    frameProfile.begin = FrameClock::now();
  int w, h;
  SDL_GetWindowSizeInPixels(window_, &w, &h);
  if (w <= 0 || h <= 0)
    return false;
  if (w != swap_.Width || h != swap_.Height)
    rebuild_ = true;
  if (rebuild_)
    resize();
  // The scene targets are shared between swapchain images. Finish the previous
  // submission before reusing them or their mapped geometry; no per-draw waits.
  waitForPreviousFrame();
  ++frameSerial;
  auto &sync = swap_.FrameSemaphores[swap_.SemaphoreIndex];
  auto result = vkAcquireNextImageKHR(device, swap_.Swapchain, UINT64_MAX,
                                      sync.ImageAcquiredSemaphore,
                                      VK_NULL_HANDLE, &swap_.FrameIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    rebuild_ = true;
    return false;
  }
  if (result == VK_SUBOPTIMAL_KHR)
    rebuild_ = true;
  else
    check(result);
  auto &f = swap_.Frames[swap_.FrameIndex];
  check(vkWaitForFences(device, 1, &f.Fence, VK_TRUE, UINT64_MAX));
  check(vkResetCommandPool(device, f.CommandPool, 0));
  command = f.CommandBuffer;
  VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command, &b));
  if (profile)
    frameProfile.ready = FrameClock::now();
  return true;
}
void Context::present(ImDrawData *data) {
  static const bool profile = enabled("NIJIGENERATE_VULKAN_PROFILE");
  if (profile)
    frameProfile.present = FrameClock::now();
  auto &f = swap_.Frames[swap_.FrameIndex];
  auto &sync = swap_.FrameSemaphores[swap_.SemaphoreIndex];
  VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  r.renderPass = swap_.RenderPass;
  r.framebuffer = f.Framebuffer;
  r.renderArea.extent = {uint32_t(swap_.Width), uint32_t(swap_.Height)};
  r.clearValueCount = 1;
  r.pClearValues = &swap_.ClearValue;
  vkCmdBeginRenderPass(command, &r, VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(data, command);
  vkCmdEndRenderPass(command);
  check(vkEndCommandBuffer(command));
  VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  s.waitSemaphoreCount = 1;
  s.pWaitSemaphores = &sync.ImageAcquiredSemaphore;
  s.pWaitDstStageMask = &wait;
  s.commandBufferCount = 1;
  s.pCommandBuffers = &command;
  s.signalSemaphoreCount = 1;
  s.pSignalSemaphores =
      &swap_.FrameSemaphores[swap_.FrameIndex].RenderCompleteSemaphore;
  check(vkResetFences(device, 1, &f.Fence));
  check(vkQueueSubmit(queue, 1, &s, f.Fence));
  lastSubmit_ = f.Fence;
  if (profile)
    frameProfile.submitted = FrameClock::now();
  VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  p.waitSemaphoreCount = 1;
  p.pWaitSemaphores =
      &swap_.FrameSemaphores[swap_.FrameIndex].RenderCompleteSemaphore;
  p.swapchainCount = 1;
  p.pSwapchains = &swap_.Swapchain;
  p.pImageIndices = &swap_.FrameIndex;
  auto result = vkQueuePresentKHR(queue, &p);
  if (profile) {
    const auto end = FrameClock::now();
    auto &t = frameProfile;
    if (t.previousEnd != FrameClock::time_point{}) {
      t.updateMs += elapsedMs(t.previousEnd, t.begin);
      t.waitMs += elapsedMs(t.begin, t.ready);
      t.recordMs += elapsedMs(t.ready, t.present);
      t.submitMs += elapsedMs(t.present, t.submitted);
      t.presentMs += elapsedMs(t.submitted, end);
      if (++t.frames == 120) {
        const double count = t.frames;
        std::fprintf(stderr,
                     "[vulkan-frame] update_ms=%.3f wait_ms=%.3f record_ms=%.3f "
                     "submit_ms=%.3f present_ms=%.3f mode=%d\n",
                     t.updateMs / count, t.waitMs / count, t.recordMs / count,
                     t.submitMs / count, t.presentMs / count, int(swap_.PresentMode));
        t.frames = 0;
        t.updateMs = t.waitMs = t.recordMs = t.submitMs = t.presentMs = 0;
      }
    }
    t.previousEnd = end;
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    rebuild_ = true;
  else
    check(result);
  swap_.SemaphoreIndex = (swap_.SemaphoreIndex + 1) % swap_.SemaphoreCount;
  command = VK_NULL_HANDLE;
}
uint32_t Context::memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const {
  VkPhysicalDeviceMemoryProperties p;
  vkGetPhysicalDeviceMemoryProperties(physical, &p);
  for (uint32_t i = 0; i < p.memoryTypeCount; ++i)
    if ((bits & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags)
      return i;
  throw std::runtime_error("No suitable Vulkan memory type");
}
void Context::destroy(Buffer &b) {
  if (b.mapped)
    vkUnmapMemory(device, b.memory);
  if (b.handle)
    vkDestroyBuffer(device, b.handle, nullptr);
  if (b.memory)
    vkFreeMemory(device, b.memory, nullptr);
  b = {};
}
void Context::ensureBuffer(Buffer &b, VkDeviceSize bytes,
                           VkBufferUsageFlags usage) {
  if (bytes <= b.capacity)
    return;
  destroy(b);
  b.capacity = std::max<VkDeviceSize>(4096, bytes * 2);
  VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  ci.size = b.capacity;
  ci.usage = usage;
  check(vkCreateBuffer(device, &ci, nullptr, &b.handle));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device, b.handle, &req);
  VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  a.allocationSize = req.size;
  a.memoryTypeIndex =
      memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  check(vkAllocateMemory(device, &a, nullptr, &b.memory));
  check(vkBindBufferMemory(device, b.handle, b.memory, 0));
  check(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
}
Image Context::createImage(uint32_t w, uint32_t h, VkFormat format,
                           VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mipLevels) {
  Image t;
  t.width = w;
  t.height = h;
  t.format = format;
  t.mipLevels = mipLevels;
  VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  i.imageType = VK_IMAGE_TYPE_2D;
  i.format = format;
  i.extent = {w, h, 1};
  i.mipLevels = mipLevels; i.arrayLayers = 1;
  i.samples = VK_SAMPLE_COUNT_1_BIT;
  i.tiling = VK_IMAGE_TILING_OPTIMAL;
  i.usage = usage;
  check(vkCreateImage(device, &i, nullptr, &t.handle));
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device, t.handle, &req);
  VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  a.allocationSize = req.size;
  a.memoryTypeIndex =
      memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(device, &a, nullptr, &t.memory));
  check(vkBindImageMemory(device, t.handle, t.memory, 0));
  VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  v.image = t.handle;
  v.viewType = VK_IMAGE_VIEW_TYPE_2D;
  v.format = format;
  v.subresourceRange = {aspect, 0, mipLevels, 0, 1};
  check(vkCreateImageView(device, &v, nullptr, &t.view));
  t.attachmentView=t.view;
  if(mipLevels>1) { v.subresourceRange.levelCount=1; check(vkCreateImageView(device,&v,nullptr,&t.attachmentView)); }
  return t;
}
void Context::destroy(Image &t) {
  if (t.ui && imgui_)
    ImGui_ImplVulkan_RemoveTexture(t.ui);
  if(t.attachmentView && t.attachmentView!=t.view) vkDestroyImageView(device,t.attachmentView,nullptr);
  if (t.view)
    vkDestroyImageView(device, t.view, nullptr);
  if (t.handle)
    vkDestroyImage(device, t.handle, nullptr);
  if (t.memory)
    vkFreeMemory(device, t.memory, nullptr);
  t = {};
}
void Context::transition(VkCommandBuffer cmd, Image &t, VkImageLayout layout,
                         VkImageAspectFlags aspect) {
  if (t.layout == layout && !t.externalOwned)
    return;
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.oldLayout = t.layout;
  b.newLayout = layout;
  b.srcQueueFamilyIndex =
      t.externalOwned ? VK_QUEUE_FAMILY_EXTERNAL : VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = t.externalOwned ? family : VK_QUEUE_FAMILY_IGNORED;
  b.image = t.handle;
  b.subresourceRange = {aspect, 0, t.mipLevels, 0, 1};
  b.srcAccessMask =
      (t.externalOwned || t.layout == VK_IMAGE_LAYOUT_UNDEFINED)
          ? 0
          : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &b);
  t.layout = layout;
  t.externalOwned = false;
}
void Context::flushSceneCommands() {
  if(!command) return;
  check(vkEndCommandBuffer(command));
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount=1; submit.pCommandBuffers=&command;
  check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
  check(vkQueueWaitIdle(queue));
  check(vkResetCommandBuffer(command,0));
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command,&begin));
}
void Context::immediate(const std::function<void(VkCommandBuffer)> &fn) {
  VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  a.commandPool = uploadPool_;
  a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  a.commandBufferCount = 1;
  VkCommandBuffer cmd;
  check(vkAllocateCommandBuffers(device, &a, &cmd));
  VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(cmd, &b));
  fn(cmd);
  check(vkEndCommandBuffer(cmd));
  VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  s.commandBufferCount = 1;
  s.pCommandBuffers = &cmd;
  check(vkQueueSubmit(queue, 1, &s, VK_NULL_HANDLE));
  check(vkQueueWaitIdle(queue));
  vkFreeCommandBuffers(device, uploadPool_, 1, &cmd);
}
void Context::upload(Image &t, const std::vector<uint8_t> &pixels) {
  if (pixels.size() != size_t(t.width) * t.height * 4)
    throw std::runtime_error("Invalid Vulkan RGBA texture size");
  Buffer staging;
  ensureBuffer(staging, pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  std::memcpy(staging.mapped, pixels.data(), pixels.size());
  immediate([&](VkCommandBuffer cmd) {
    transition(cmd, t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageExtent = {t.width, t.height, 1};
    vkCmdCopyBufferToImage(cmd, staging.handle, t.handle, t.layout, 1, &c);
    transition(cmd, t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });
  destroy(staging);
  if(t.mipLevels>1) generateMipmaps(t);
}
void Context::generateMipmaps(Image& image) {
  if(image.mipLevels<=1) return;
  immediate([&](VkCommandBuffer cmd) { generateMipmaps(cmd,image); });
}
void Context::generateMipmaps(VkCommandBuffer cmd,Image& image) {
  if(image.mipLevels<=1) return;
    transition(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    int32_t w=int32_t(image.width),h=int32_t(image.height);
    for(uint32_t level=1;level<image.mipLevels;++level) {
      VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      b.image=image.handle; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,level-1,1,0,1};
      b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
      b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
      vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
      const auto nw=std::max(1,w/2),nh=std::max(1,h/2);
      VkImageBlit blit{};
      blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level-1,0,1}; blit.srcOffsets[1]={w,h,1};
      blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,1}; blit.dstOffsets[1]={nw,nh,1};
      vkCmdBlitImage(cmd,image.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,image.handle,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
      b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT; b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
      w=nw; h=nh;
    }
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.image=image.handle; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,image.mipLevels-1,1,0,1};
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
    image.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

}
std::vector<uint8_t> Context::read(Image &t) {
  Buffer b;
  auto bytes = size_t(t.width) * t.height * 4;
  ensureBuffer(b, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  immediate([&](VkCommandBuffer cmd) {
    auto old = t.layout;
    transition(cmd, t, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageExtent = {t.width, t.height, 1};
    vkCmdCopyImageToBuffer(cmd, t.handle, t.layout, b.handle, 1, &c);
    transition(cmd, t, old);
  });
  std::vector<uint8_t> result(bytes);
  std::memcpy(result.data(), b.mapped, bytes);
  destroy(b);
  return result;
}
ImTextureID Context::texture(Image &t) {
  if (!t.ui)
    t.ui = ImGui_ImplVulkan_AddTexture(
        sampler, t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return reinterpret_cast<ImTextureID>(t.ui);
}
} // namespace nijigenerate::backend::vulkan
