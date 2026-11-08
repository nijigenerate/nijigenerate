#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

namespace nijigenerate::backend::vulkan {
class Backend;
void check(VkResult result);
struct Buffer {
  VkBuffer handle{};
  VkDeviceMemory memory{};
  VkDeviceSize capacity{};
  void *mapped{};
};
struct Image {
  VkImage handle{};
  VkDeviceMemory memory{};
  VkImageView view{}, attachmentView{};
  VkDescriptorSet ui{};
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  uint32_t width{}, height{}, mipLevels = 1;
  // Keeps an imported platform allocation alive until its Vulkan image is
  // freed.
  std::shared_ptr<void> externalOwner;
  void *sharedHandle{};
  bool externalOwned = false;
};
// Owns the SDL surface, Vulkan device, swapchain and ImGui renderer. All GPU
// objects are used and retired on the window's rendering thread.
class Context {
  friend class Backend;
  Backend *renderBackend_{};
  SDL_Window *window_{};
  ImGui_ImplVulkanH_Window swap_{};
  VkCommandPool uploadPool_{};
  VkFence lastSubmit_{};
  bool rebuild_ = false, imgui_ = false;
  int throttle_ = 1;
  void release();

public:
  VkInstance instance{};
  VkPhysicalDevice physical{};
  VkDevice device{};
  VkQueue queue{};
  uint32_t family{};
  VkDescriptorPool descriptors{};
  VkSampler sampler{};
  VkCommandBuffer command{};
  float maxAnisotropy = 1;
  bool advancedBlendSupported = false;
  bool independentBlendSupported = false;
  bool advancedBlendCoherent = false;
  bool externalWin32Supported = false;
  explicit Context(SDL_Window *window);
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
  void initializeImGui();
  void shutdownImGui();
  void resize();
  void setThrottle(int rate);
  bool beginFrame();
  void present(ImDrawData *data);
  void idle();
  uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const;
  void ensureBuffer(Buffer &, VkDeviceSize, VkBufferUsageFlags);
  void destroy(Buffer &);
  Image createImage(uint32_t, uint32_t, VkFormat, VkImageUsageFlags,
                    VkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t mipLevels = 1);
  void destroy(Image &);
  void transition(VkCommandBuffer, Image &, VkImageLayout,
                  VkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
  void immediate(const std::function<void(VkCommandBuffer)> &);
  void upload(Image &, const std::vector<uint8_t> &);
  void generateMipmaps(Image &);
  void generateMipmaps(VkCommandBuffer,Image &);
  std::vector<uint8_t> read(Image &);
  ImTextureID texture(Image &);
  const char* apiName() const { return "Vulkan"; }
  std::string deviceName() const {
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(physical,&props); return props.deviceName;
  }
  int maxTextureSize() const {
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(physical,&props); return int(props.limits.maxImageDimension2D);
  }
  int width() const { return swap_.Width; }
  int height() const { return swap_.Height; }
};
Context &context();
} // namespace nijigenerate::backend::vulkan
