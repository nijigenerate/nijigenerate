#include <nijigenerate/widgets/shadow.hpp>
#include <nijigenerate/widgets/shadow_internal.hpp>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace nijigenerate::widgets {
ImDrawList* ngCreateWindowDrawList() { return IM_NEW(ImDrawList)(ImGui::GetDrawListSharedData()); }
void ngDestroyWindowDrawList(ImDrawList* drawList) { IM_DELETE(drawList); }
namespace {
template<class T> bool sharedStorage(const ImVector<T>& first, const ImVector<T>& second) {
    return first.Data && first.Data == second.Data;
}

void requireDistinct(const ImDrawList* self, const ImDrawList* other) {
    if (!self || !other || self == other || !self->_Data || self->_Data != other->_Data ||
        sharedStorage(self->CmdBuffer, other->CmdBuffer) ||
        sharedStorage(self->IdxBuffer, other->IdxBuffer) ||
        sharedStorage(self->VtxBuffer, other->VtxBuffer) ||
        sharedStorage(self->_Path, other->_Path) ||
        sharedStorage(self->_TextureIdStack, other->_TextureIdStack) ||
        sharedStorage(self->_ClipRectStack, other->_ClipRectStack) ||
        sharedStorage(self->_CallbacksDataBuf, other->_CallbacksDataBuf) ||
        sharedStorage(self->_Splitter._Channels, other->_Splitter._Channels))
        throw std::invalid_argument("Window shadow requires distinct draw lists with the same shared data");
}

int combinedSize(int first, int second, int spare = 0) {
    if (first < 0 || second < 0 || spare < 0 ||
        std::uint64_t(first) + std::uint64_t(second) + std::uint64_t(spare) >
            std::uint64_t(std::numeric_limits<int>::max()))
        throw std::length_error("Window shadow exceeds ImVector capacity");
    return first + second + spare;
}

void requireComplete(const ImDrawList& list) {
    if (list.VtxBuffer.Size < 0 || list.IdxBuffer.Size < 0 || list.CmdBuffer.Size < 0)
        throw std::invalid_argument("Window shadow requires valid draw buffers");
    if ((list.VtxBuffer.Size && list._VtxWritePtr != list.VtxBuffer.Data + list.VtxBuffer.Size) ||
        (list.IdxBuffer.Size && list._IdxWritePtr != list.IdxBuffer.Data + list.IdxBuffer.Size) ||
        std::uint64_t(list._CmdHeader.VtxOffset) + list._VtxCurrentIdx !=
            std::uint64_t(list.VtxBuffer.Size))
        throw std::invalid_argument("Window shadow requires completed draw primitives");
    if (!(list.Flags & ImDrawListFlags_AllowVtxOffset) && list._CmdHeader.VtxOffset != 0)
        throw std::invalid_argument("Window shadow requires a valid absolute vertex segment");
}

void requireWindowLevel(const ImDrawList* list) {
    const auto* context = ImGui::GetCurrentContext();
    const auto* window = context ? context->CurrentWindow : nullptr;
    if (!window || window->DrawList != list) return;
    const auto* table = context->CurrentTable;
    if (window->DC.CurrentColumns ||
        (table && (table->InnerWindow == window || table->OuterWindow == window) &&
            table->DrawSplitter && table->DrawSplitter->_Count > 1))
        throw std::invalid_argument("Draw window shadows before table or column channel splitting");
}

const ImVector<ImDrawCmd>& firstChannelCommands(const ImDrawList& list) {
    if (list._Splitter._Count > 1 && list._Splitter._Current != 0)
        return list._Splitter._Channels[0]._CmdBuffer;
    return list.CmdBuffer;
}
}

namespace detail {
void prepend(ImDrawList* self, ImDrawList* other) {
    requireDistinct(self, other);
    requireComplete(*self);
    requireComplete(*other);
    requireWindowLevel(self);
    if (other->_Splitter._Count > 1 || other->_CallbacksDataBuf.Size != 0)
        throw std::invalid_argument("Window shadow prefix must be completed geometry without callbacks");

    const bool vertexOffsets = (self->Flags & ImDrawListFlags_AllowVtxOffset) != 0;
    const int originalVertices = self->VtxBuffer.Size;
    const int vertices = combinedSize(originalVertices, other->VtxBuffer.Size);
    if (!vertexOffsets && std::uint64_t(vertices) > std::numeric_limits<ImDrawIdx>::max())
        throw std::length_error("Window shadow exceeds the draw list's absolute index range");

    // Stage the prefix before changing destination state. Only command order,
    // not vertex-buffer order, determines whether the shadow is behind the UI.
    ImVector<ImDrawCmd> commands;
    ImVector<ImDrawIdx> indices;
    indices.resize(other->IdxBuffer.Size);
    if (indices.Size)
        std::memcpy(indices.Data, other->IdxBuffer.Data, std::size_t(indices.Size) * sizeof(ImDrawIdx));
    unsigned int expectedIndex = 0;
    for (const auto& source : other->CmdBuffer) {
        if (source.UserCallback)
            throw std::invalid_argument("Window shadow prefix cannot contain callbacks");
        if (!source.ElemCount) continue;
        if (source.IdxOffset != expectedIndex || source.IdxOffset > unsigned(indices.Size) ||
            source.ElemCount > unsigned(indices.Size) - source.IdxOffset)
            throw std::invalid_argument("Window shadow prefix has invalid index ranges");
        expectedIndex += source.ElemCount;
        auto command = source;
        for (unsigned int i = source.IdxOffset; i < expectedIndex; ++i) {
            const auto vertex = std::uint64_t(source.VtxOffset) + indices[int(i)];
            if (vertex >= std::uint64_t(other->VtxBuffer.Size))
                throw std::invalid_argument("Window shadow prefix has an invalid vertex index");
            if (!vertexOffsets) indices[int(i)] = ImDrawIdx(originalVertices + vertex);
        }
        command.VtxOffset = vertexOffsets ? unsigned(originalVertices) + source.VtxOffset : 0;
        commands.push_back(command);
    }
    if (expectedIndex != unsigned(indices.Size))
        throw std::invalid_argument("Window shadow prefix has unclaimed indices");
    if (!commands.Size) return;

    auto& splitter = self->_Splitter;
    const int originalChannel = splitter._Current;
    const bool split = splitter._Count > 1;
    auto& firstCommands = split && originalChannel != 0 ? splitter._Channels[0]._CmdBuffer : self->CmdBuffer;
    auto& firstIndices = split && originalChannel != 0 ? splitter._Channels[0]._IdxBuffer : self->IdxBuffer;
    const int indexCount = combinedSize(firstIndices.Size, indices.Size);
    const int commandCapacity = combinedSize(firstCommands.Size, commands.Size, 3);
    firstCommands.reserve(commandCapacity);
    firstIndices.reserve(indexCount);
    self->VtxBuffer.reserve(vertices);
    if (split) splitter.SetCurrentChannel(self, 0);
    if (!self->CmdBuffer.Size || self->CmdBuffer.back().UserCallback) self->AddDrawCmd();

    // Existing vertex indices and bases, including queued channels and callback
    // commands, remain valid because their vertices stay at the same positions.
    self->VtxBuffer.resize(vertices);
    if (other->VtxBuffer.Size)
        std::memcpy(self->VtxBuffer.Data + originalVertices, other->VtxBuffer.Data,
            std::size_t(other->VtxBuffer.Size) * sizeof(ImDrawVert));
    self->_VtxWritePtr = self->VtxBuffer.Data + vertices;
    for (auto& command : self->CmdBuffer) command.IdxOffset += unsigned(indices.Size);
    self->_IdxWritePtr = prepend(self->IdxBuffer, indices);
    prepend(self->CmdBuffer, commands);

    if (vertexOffsets) {
        // A genuine offset-capable backend can continue in a fresh local
        // segment. No shifted absolute count can overflow a 16-bit index.
        self->_CmdHeader.VtxOffset = unsigned(vertices);
        self->_OnChangedVtxOffset();
    } else {
        self->_VtxCurrentIdx = unsigned(vertices);
    }
    if (split) splitter.SetCurrentChannel(self, originalChannel);
    // The destination's path, texture/clip stacks and callback-data storage are
    // live UI state, not part of the completed shadow prefix.
}
}
void ngRenderWindowShadow(ImDrawList* drawList, ImRect area, float falloff, float startShade) {
    auto* output=ImGui::GetWindowDrawList();
    requireDistinct(output, drawList);
    requireWindowLevel(output);
    const auto& firstCommands = firstChannelCommands(*output);
    const auto clip = firstCommands.Size ? firstCommands[0].ClipRect : output->_CmdHeader.ClipRect;
    const auto texture = firstCommands.Size ? firstCommands[0].TextureId : output->_CmdHeader.TextureId;
    drawList->_ResetForNewFrame();
    drawList->PushTextureID(texture);
    drawList->PushClipRect({clip.x, clip.y}, {clip.z, clip.w}, false);
    auto& style=ImGui::GetStyle();
    const float inset=style.WindowRounding;
    const auto innerColor=ImGui::GetColorU32(ImVec4(0,0,0,startShade));
    const auto outerColor=ImGui::GetColorU32(ImVec4(0,0,0,0));
    const ImVec2 innerMin(area.Min.x-inset,area.Min.y-inset);
    const ImVec2 innerMax(area.Max.x+inset,area.Max.y+inset);
    const ImVec2 realMin(innerMin.x+falloff,innerMin.y+falloff);
    const ImVec2 realMax(innerMax.x-falloff,innerMax.y-falloff);
    drawList->AddRectFilled(innerMin,innerMax,innerColor);
    drawList->AddRectFilledMultiColor({realMax.x,realMax.y},innerMax,outerColor,outerColor,innerColor,outerColor);
    drawList->AddRectFilledMultiColor({realMin.x,realMax.y},{innerMin.x,innerMax.y},outerColor,outerColor,innerColor,outerColor);
    drawList->AddRectFilledMultiColor({realMax.x,realMin.y},{innerMax.x,innerMin.y},outerColor,outerColor,innerColor,outerColor);
    drawList->AddRectFilledMultiColor(realMin,innerMin,outerColor,outerColor,innerColor,outerColor);
    drawList->AddRectFilledMultiColor({realMax.x,innerMin.y},innerMax,outerColor,innerColor,innerColor,outerColor);
    drawList->AddRectFilledMultiColor(innerMin,{realMin.x,innerMax.y},innerColor,outerColor,outerColor,innerColor);
    drawList->AddRectFilledMultiColor({innerMin.x,realMax.y},innerMax,outerColor,outerColor,innerColor,innerColor);
    drawList->AddRectFilledMultiColor(innerMin,{innerMax.x,realMin.y},innerColor,innerColor,outerColor,outerColor);
    detail::prepend(output,drawList);
}
}
