#pragma once

#include <nijigenerate/widgets/modal/base.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/viewport/vertex/automesh/automesh.hpp>
#include <nodes/part.hpp>
#include <compat/source_mutex.hpp>
#include <cstdint>
#include <memory>
#include <functional>

namespace nijigenerate::windows::automeshbatch {

class AutoMeshBatchWindow : public widgets::modal::Modal,
    public std::enable_shared_from_this<AutoMeshBatchWindow> {
    using Node = nicxlive::core::nodes::Node;
    using Part = nicxlive::core::nodes::Part;
    using Deformable = nicxlive::core::nodes::Deformable;
    using IncMesh = viewport::common::mesh::IncMesh;
    using Processor = viewport::vertex::automesh::AutoMeshProcessor;
    using NodeRef = std::shared_ptr<Node>;
    using MeshRef = std::shared_ptr<IncMesh>;
    using NodeList = compat::Slice<NodeRef>;
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
    template<class Value> using UuidMap = nicxlive::core::common::ResourceKeyMap<
        std::uint32_t, Value, UuidHash, std::equal_to<std::uint32_t>>;
    struct InitialState;
    struct ProcessingThread;

    compat::RetainedStringSlice nodeFilter;
    static constexpr float PreviewSize = 128;
    NodeList nodes;
    UuidMap<MeshRef> meshes;
    UuidMap<bool> selected;
    NodeRef active;
    enum class Status { Waiting, Running, Succeeded, Failed };
    UuidMap<Status> status;
    std::shared_ptr<ProcessingThread> processingThread;
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> gcMutex;
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> batchMutex;
    bool canceled = false;
    bool closeAfterProcessing = false;
    std::shared_ptr<Processor> activeProcessor;
    compat::Slice<std::shared_ptr<Part>> lockedParts;
    bool selectAll = false;
    enum class ToggleAction { NoAction, None, All };
    ToggleAction toggleAction = ToggleAction::NoAction;

    static InitialState captureInitialState();
    explicit AutoMeshBatchWindow(InitialState initial);
    bool isProcessing();
    void setStatus(std::uint32_t uuid, Status value);
    bool getStatus(std::uint32_t uuid, Status& value);
    void setMesh(std::uint32_t uuid, MeshRef mesh);
    MeshRef getMesh(std::uint32_t uuid);
    void clearStatuses();
    void apply();
    nicxlive::core::math::Vec4 previewImage(std::shared_ptr<Part> part, ImVec2 centerPos,
        float previewSize, ImVec2 uv0 = {0, 0}, ImVec2 uv1 = {1, 1},
        ImVec4 tintColor = {1, 1, 1, 1}, MeshRef mesh = {});
    NodeList getFilteredNodes();
    void handleKeyboardNavigation(NodeList filteredNodes);
    void renderNodeList(NodeList filteredNodes);
    void treeView();
    static bool shouldBeSelected(NodeRef node);
    static bool isApplicable(NodeRef node);

protected:
    void prepareSourceClassInfo() override {
        widgets::modal::Modal::prepareSourceClassInfo();
        registerSourceClassInfo<AutoMeshBatchWindow>(
            "nijigenerate.windows.automeshbatch.AutoMeshBatchWindow");
    }
    // Native Fiber cleanup/thread effects remain explicit validation gates.
    virtual void runBatch(compat::Slice<std::shared_ptr<Deformable>> targets,
        compat::Slice<MeshRef> meshList, std::shared_ptr<Processor> processor);
    void onBeginUpdate() override;
    void onUpdate() override;
    virtual void close();

public:
    AutoMeshBatchWindow();
    ~AutoMeshBatchWindow() override;
};

} // namespace nijigenerate::windows::automeshbatch

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::automeshbatch::AutoMeshBatchWindow> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.automeshbatch.AutoMeshBatchWindow)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshBatchWindow)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.automeshbatch.AutoMeshBatchWindow";
    inline static constexpr char unqualifiedElement[] = "AutoMeshBatchWindow";
};
template<> struct SourceObjectTypeNames<::nijigenerate::windows::automeshbatch::AutoMeshBatchWindow> {
    inline static constexpr char className[] = "nijigenerate.windows.automeshbatch.AutoMeshBatchWindow";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.automeshbatch.AutoMeshBatchWindow)";
};
}
