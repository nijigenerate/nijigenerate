#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace nijigenerate::backend::metal {
struct Image {
    std::shared_ptr<void> owner;
    uint32_t width{}, height{}, mipLevels=1;
};
class Context {
public:
    struct Impl;
    std::unique_ptr<Impl> p;
    float maxAnisotropy=16;
    bool advancedBlendSupported=true, advancedBlendCoherent=true;
    explicit Context(SDL_Window*);
    ~Context();
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
    void initializeImGui();
    void shutdownImGui();
    void newFrame();
    void resize();
    void setThrottle(int);
    bool beginFrame();
    void present(ImDrawData*);
    void idle();
    bool recording() const;
    void immediate(const std::function<void()>&);
    Image createImage(uint32_t,uint32_t,uint32_t mipLevels=1,bool depth=false,bool memoryless=false);
    void destroy(Image&);
    void upload(Image&,const std::vector<uint8_t>&);
    void generateMipmaps(Image&);
    std::vector<uint8_t> read(Image&);
    ImTextureID texture(Image&);
    int width() const;
    int height() const;
    int maxTextureSize() const;
    std::string deviceName() const;
    const char* apiName() const { return "Metal"; }
};
Context& context();
}
