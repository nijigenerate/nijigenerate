#pragma once
#include <shader.hpp>
#include <render/part_draw_packet.hpp>
namespace nijigenerate::backend::runtime_gl {
extern std::shared_ptr<nicxlive::core::Shader> partShader;
extern std::shared_ptr<nicxlive::core::Shader> partShaderStage1, partShaderStage2, partMaskShader;
extern int gopacity, gMultColor, gScreenColor, gEmissionStrength, mvp, offset;
extern int gs1mvp, gs1offset, gs1opacity, gs1MultColor, gs1ScreenColor;
extern int gs2mvp, gs2offset, gs2opacity, gs2MultColor, gs2ScreenColor, gs2EmissionStrength;
extern int mmvp, mthreshold;
extern bool partBackendInitialized;
void oglInitPartBackendResources();
void oglResetBoundAlbedo();
void oglDrawPartPacket(const nicxlive::core::nodes::PartDrawPacket& packet);
void oglExecutePartPacket(const nicxlive::core::nodes::PartDrawPacket& packet);
}
