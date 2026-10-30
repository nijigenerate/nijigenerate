#include "automeshbatch.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/platform/automesh_batch_stack.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nodes/projectable.hpp>
#include <puppet.hpp>
#include <resource.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <array>
#include <exception>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <system_error>
#include <utility>

namespace nijigenerate::windows::automeshbatch {
using namespace widgets;
using nicxlive::core::nodes::Drawable;
namespace {

template<class Operation> decltype(auto) allocateText(Operation&& operation) {
    // Only pure byte or owner allocation belongs inside this boundary.
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
std::optional<std::string> zeroArgumentLabel(std::string_view pattern) {
    // Source Text receives no arguments; its literal %% renders as one percent.
    bool hasEscape = false;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] != '%') continue;
        if (pattern.size() - i < 2 || pattern[i + 1] != '%') return {};
        hasEscape = true;
        ++i;
    }
    if (!hasEscape) return {};
    std::string result;
    const auto write = [&](std::string_view bytes) {
        allocateText([&] { result.append(bytes); });
    };
    const auto sink = allocateText([&] { return core::translation::TypedFormatSink(write); });
    core::translation::ngFormatTypedTo(sink, pattern,
        std::span<const core::translation::TypedFormatArgument>{});
    return std::optional<std::string>{std::move(result)};
}
std::uint32_t nodeUuid(const std::shared_ptr<nicxlive::core::nodes::Node>& node) {
    return compat::ngRequireObject(node)->getUUID();
}
std::string iconName(const std::shared_ptr<nicxlive::core::nodes::Node>& node) {
    const auto icon = [&] {
        const auto receiver = compat::ngRequireObject(node);
        const auto iconValue = utils::incTypeIdToIcon(receiver->typeId());
        return allocateText([&] { return std::string(iconValue); });
    }();
    const auto name = compat::ngRequireObject(node)->getName();
    return allocateText([&] { return icon + std::string(name.view()); });
}
compat::SourceCString uuidLabel(std::string_view pattern, std::uint32_t uuid) {
    const std::array<core::translation::TypedFormatArgument,1> arguments{
        core::translation::ngFormatArgument(uuid)};
    auto formatted = core::translation::ngFormatTyped(pattern, arguments);
    const auto label = allocateText([&] { return compat::RetainedString(std::move(formatted)); });
    compat::SourceCString result{compat::RetainedStringSlice(label)};
    compat::detail::ngSourceCStringPostconditions(label.view(), result.data());
    return result;
}
// Lower synchronized(mutex) with a real retained monitor, not a noexcept guard.
// Enter failure does not unlock; exit failure follows the source finally chain.
template<class Body> void synchronizedMutex(
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> mutex, Body&& body) {
    const auto monitor = compat::ngRequireObject(std::move(mutex));
    monitor->lock();
    compat::ngRunWithScopeExit(std::forward<Body>(body), [monitor] { monitor->unlock(); });
}
}

struct AutoMeshBatchWindow::InitialState {
    NodeList nodes;
    UuidMap<MeshRef> meshes;
    UuidMap<bool> selected;
};

// A real OS worker with ordinary C++ ownership and thread cleanup.
struct AutoMeshBatchWindow::ProcessingThread {
    std::thread worker;
    std::atomic<bool> running{false};
    std::exception_ptr failure;
    ~ProcessingThread() { if (worker.joinable()) worker.detach(); }
    void join() {
        if (worker.joinable()) {
            try {
                worker.join();
            }
            catch(const std::system_error&) {
                throw compat::ThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
                    compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"),
#if defined(_WIN32)
                    606);
#else
                    617);
#endif
            }
        }
        if (failure) std::rethrow_exception(failure);
    }
};

AutoMeshBatchWindow::InitialState AutoMeshBatchWindow::captureInitialState() {
    InitialState initial;
    auto puppet = project::incActivePuppet();
    const auto root = compat::ngRequireObject(puppet)->root;
    if (!root) compat::ngFailNullAccess(false);
    auto capturedNodes = puppet->findNodesType<Node>(root);
    if (!capturedNodes.isNull()) {
        initial.nodes = NodeList::retainedView(capturedNodes.retentionOwner(),
            capturedNodes.data(), capturedNodes.size());
    }
    auto retained = initial.nodes;
    for (auto node : retained) {
        (void)initial.selected[nodeUuid(node)];
        const auto selectedKey=nodeUuid(node);
        initial.selected[selectedKey] = shouldBeSelected(node);
        if (isApplicable(node)) {
            auto part = std::dynamic_pointer_cast<Part>(node);
            const auto meshKey=nodeUuid(node);
            auto& sourceMesh = compat::ngRequireObject(part)->Drawable::getMesh();
            const auto sourceMeshOwner = part->mesh;
            initial.meshes[meshKey] = IncMesh::create(sourceMesh, sourceMeshOwner);
        }
    }
    return initial;
}

AutoMeshBatchWindow::AutoMeshBatchWindow() : AutoMeshBatchWindow(captureInitialState()) {}
AutoMeshBatchWindow::AutoMeshBatchWindow(InitialState initial)
    : Modal(tr("Automesh Batching"), true),
      nodes(std::move(initial.nodes)), meshes(std::move(initial.meshes)), selected(std::move(initial.selected)) {
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    gcMutex = std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
    batchMutex = std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
}
AutoMeshBatchWindow::~AutoMeshBatchWindow() = default;
bool AutoMeshBatchWindow::isProcessing() { return static_cast<bool>(processingThread); }
void AutoMeshBatchWindow::setStatus(std::uint32_t uuid, Status value) {
    synchronizedMutex(batchMutex, [&] { status[uuid] = value; });
}
bool AutoMeshBatchWindow::getStatus(std::uint32_t uuid, Status& value) {
    value = Status::Waiting;
    bool present = false;
    synchronizedMutex(batchMutex, [&] {
        const auto found = status.find(uuid);
        if (found == status.end()) return;
        value = found->second;
        present = true;
    });
    return present;
}
void AutoMeshBatchWindow::setMesh(std::uint32_t uuid, MeshRef mesh) {
    synchronizedMutex(batchMutex, [&] { meshes[uuid] = mesh; });
}
AutoMeshBatchWindow::MeshRef AutoMeshBatchWindow::getMesh(std::uint32_t uuid) {
    MeshRef result;
    synchronizedMutex(batchMutex, [&] {
        const auto found = meshes.find(uuid);
        if (found != meshes.end()) result = found->second;
    });
    return result;
}
void AutoMeshBatchWindow::clearStatuses() { synchronizedMutex(batchMutex, [&] { status.clear(); }); }

void AutoMeshBatchWindow::apply() {
    bool grouped = false;
    compat::ngRunWithScopeExit([&] {
        auto retained = nodes;
        for (auto node : retained) {
            auto part = std::dynamic_pointer_cast<Part>(node);
            Status nodeStatus;
            auto mesh = getMesh(compat::ngRequireObject(node)->getUUID());
            if (!isApplicable(node) || !mesh || !getStatus(compat::ngRequireObject(node)->getUUID(), nodeStatus) ||
                nodeStatus != Status::Succeeded)
                continue;
            compat::RetainedString message;
            if (!grouped) { core::actionstack::incActionPushGroup(); grouped = true; }
            viewport::vertex::ngApplyDrawableMeshFromCommand(part, mesh, message);
        }
    }, [&] { if (grouped) core::actionstack::incActionPopGroup(); });
}

nicxlive::core::math::Vec4 AutoMeshBatchWindow::previewImage(std::shared_ptr<Part> part,
        ImVec2 center, float previewSize, ImVec2 uv0, ImVec2 uv1, ImVec4 tint, MeshRef mesh) {
    const auto texture = [&](std::size_t sourceLine) {
        const auto receiver = compat::ngRequireObject(part);
        const auto textures = receiver->textures;
        if (textures.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, textures.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/automeshbatch.d"), sourceLine);
        return compat::ngRequireObject(textures[0]);
    };
    if (texture(115)->width() < previewSize && texture(115)->height() < previewSize) {
        const auto width = texture(116)->width();
        const auto height = texture(116)->height();
        previewSize = static_cast<float>(std::max(width, height));
    }
    const float widthScale = previewSize / static_cast<float>(texture(118)->width());
    const float heightScale = previewSize / static_cast<float>(texture(119)->height());
    const float scale = heightScale < widthScale ? heightScale : widthScale;
    const auto boundsWidth = texture(122)->width() * scale;
    const auto boundsHeight = texture(122)->height() * scale;
    nicxlive::core::math::Vec4 bounds{0, 0, boundsWidth, boundsHeight};
    if (widthScale > heightScale) bounds.x = (previewSize - bounds.z) / 2;
    else if (widthScale < heightScale) bounds.y = (previewSize - bounds.w) / 2;
    const auto tl = ImGui::GetCursorPos();
    ImGui::ItemSize({previewSize, previewSize});
    ImGui::SetCursorPos({tl.x + center.x - previewSize / 2 + bounds.x,
        tl.y + center.y - previewSize / 2 + bounds.y});
    const auto screen = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(texture(138)->getTextureId()), {bounds.z, bounds.w}, uv0, uv1, tint);
    if (mesh) {
        auto draw = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(ImVec4(.7f, .7f, .7f, 1));
        const float cx = static_cast<float>(texture(145)->width() / 2);
        const float cy = static_cast<float>(texture(145)->height() / 2);
        auto vertices = mesh->vertices;
        for (auto vertex : vertices) {
            if (!vertex) compat::ngFailNullAccess(false);
            auto connections = vertex->connections;
            for (auto connected : connections) {
                const auto p1 = vertex->position;
                if (!connected) compat::ngFailNullAccess(false);
                const auto p2 = connected->position;
                const ImVec2 first{screen.x + (p1.x + cx) * scale, screen.y + (p1.y + cy) * scale};
                const ImVec2 second{screen.x + (p2.x + cx) * scale, screen.y + (p2.y + cy) * scale};
                compat::ngRequireObject(draw)->AddLine(first, second, color, 1);
            }
        }
    }
    return bounds;
}

AutoMeshBatchWindow::NodeList AutoMeshBatchWindow::getFilteredNodes() {
    if (nodeFilter.empty()) return nodes;
    NodeList filtered;
    auto retained = nodes;
    for (auto node : retained) {
        if (!nodeFilter.empty()) {
            const auto name = ext::nodes::ngRetainNodeName(compat::ngRequireObject(node)->getName());
            const auto loweredName = io::imageexport::detail::lowerExtension(name);
            const auto loweredFilter = io::imageexport::detail::lowerExtension(nodeFilter);
            if (loweredName.view().find(loweredFilter.view()) == std::string_view::npos) continue;
        }
        filtered.append(node);
    }
    return filtered;
}
void AutoMeshBatchWindow::handleKeyboardNavigation(NodeList filtered) {
    if (ImGui::IsAnyItemActive() || filtered.empty()) return;
    std::ptrdiff_t index = -1;
    if (active) {
        const auto needle = active;
        for (std::size_t i = 0; i < filtered.size(); ++i)
            if (nicxlive::core::ngResourceObjectEquals(filtered[i], needle)) {
                index = static_cast<std::ptrdiff_t>(i);
                break;
            }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        index = index <= 0 ? static_cast<std::ptrdiff_t>(filtered.size() - 1) : index - 1;
        active = filtered[static_cast<std::size_t>(index)];
    } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        // The D comparison converts -1 to size_t and therefore wraps to zero.
        index = static_cast<std::size_t>(index) >= filtered.size() - 1 ? 0 : index + 1;
        active = filtered[static_cast<std::size_t>(index)];
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Space, true) && active && isApplicable(active)) {
        auto destination=selected;
        const auto writeKey=nodeUuid(active);
        const auto selectedMap = selected;
        const auto readKey = nodeUuid(active);
        const auto found = selectedMap.find(readKey);
        const bool previous = found != selectedMap.end() ? found->second : false;
        destination[writeKey] = !previous;
    }
}
void AutoMeshBatchWindow::renderNodeList(NodeList filtered) {
    selectAll = true;
    for (std::size_t i = 0; i < filtered.size(); ++i) {
        auto& node = filtered[i];
        ImGui::PushID(static_cast<int>(i));
        const auto itemLabel=uuidLabel("###%x",nodeUuid(node));
        const auto isActive=nicxlive::core::ngResourceObjectEquals(active,node);
        if (ImGui::Selectable(itemLabel.data(),isActive,
                ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowOverlap)) active = node;
        ImGui::SameLine(0, 0);
        if (isApplicable(node)) {
            if (toggleAction == ToggleAction::None) selected[nodeUuid(node)] = false;
            else if (toggleAction == ToggleAction::All) selected[nodeUuid(node)] = true;
            {
                const auto selectedMap = selected;
                if (!selectedMap.at(nodeUuid(node))) selectAll = false;
            }
            const auto checkboxLabel = uuidLabel("###check%x", nodeUuid(node));
            auto selectedMap = selected;
            const auto selectedKey = nodeUuid(node);
            auto selectedCell = selectedMap[selectedKey];
            bool& selectedValue = selectedCell;
            ngCheckbox(checkboxLabel.data(), &selectedValue);
            ImGui::SameLine(0, 0);
        } else ImGui::SameLine(30, 0);
        Status nodeStatus;
        if (getStatus(nodeUuid(node), nodeStatus)) {
            switch (nodeStatus) {
            case Status::Waiting: ImGui::TextColored({.8f, .4f, 0, 1}, "%s", "\356\275\212"); break;
            case Status::Running: ImGui::TextColored({0, .4f, .8f, 1}, "%s", "\356\207\204"); break;
            case Status::Succeeded: ImGui::TextColored({0, .9f, 0, 1}, "%s", "\356\244\257"); break;
            case Status::Failed: ImGui::TextColored({.9f, 0, 0, 1}, "%s", "\357\242\266"); break;
            }
        }
        ImGui::SameLine(0, 0);
        auto displayText = iconName(node);
        const auto displayedName = allocateText([&] { return compat::RetainedString(std::move(displayText)); });
        const compat::SourceCString terminatedName{compat::RetainedStringSlice(displayedName)};
        compat::detail::ngSourceCStringPostconditions(displayedName.view(), terminatedName.data());
        const auto formattedName = zeroArgumentLabel(std::string_view(terminatedName.data()));
        ImGui::Text("%s", formattedName ? formattedName->c_str() : terminatedName.data());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip(); ngText(iconName(node));
            if (auto part = std::dynamic_pointer_cast<Part>(node))
                previewImage(part, {PreviewSize / 2, PreviewSize / 2}, PreviewSize);
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    toggleAction = ToggleAction::NoAction;
}
void AutoMeshBatchWindow::treeView() {
    auto filtered = getFilteredNodes();
    handleKeyboardNavigation(filtered);
    renderNodeList(filtered);
}
bool AutoMeshBatchWindow::shouldBeSelected(NodeRef node) {
    if (!std::dynamic_pointer_cast<Part>(node)) return false;
    auto projectable = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(node);
    return !projectable || !projectable->autoResizedMesh;
}
bool AutoMeshBatchWindow::isApplicable(NodeRef node) { return static_cast<bool>(std::dynamic_pointer_cast<Part>(node)); }

void AutoMeshBatchWindow::runBatch(compat::Slice<std::shared_ptr<Deformable>> targets,
        compat::Slice<MeshRef> meshList, std::shared_ptr<Processor> processor) {
    struct BatchFrame {
        std::shared_ptr<AutoMeshBatchWindow> self;
        std::shared_ptr<Deformable> currentTarget;
    };
    // D escaping delegates retain their lexical frame and the original this.
    const auto frame = std::allocate_shared<BatchFrame>(
        nicxlive::core::d_runtime::SourceAllocator<BatchFrame>{}, BatchFrame{shared_from_this(), {}});
    const auto callback = [frame](std::shared_ptr<Deformable> deformable, MeshRef mesh) {
        frame->currentTarget = deformable;
        const auto self=frame->self;
        if (!mesh) self->setStatus(nodeUuid(deformable), Status::Running);
        else if (mesh->vertices.size() >= 3) {
            if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(deformable)) {
                self->setStatus(nodeUuid(deformable), Status::Succeeded);
                self->setMesh(nodeUuid(drawable), mesh);
            } else self->setStatus(nodeUuid(deformable), Status::Failed);
        } else self->setStatus(nodeUuid(deformable), Status::Failed);
        bool result = false;
        synchronizedMutex(self->gcMutex, [&] { result = self->canceled; });
        return !result;
    };
    const auto work = [processor, targets, meshList, callback] {
        compat::ngRequireObject(processor)->autoMesh(targets, meshList, false, 0, false, 0, callback);
    };
    core::tasks::TaskFiber fiber(work, platform::ngAutoMeshBatchFiberStackBytes());
    while (fiber.state() != core::tasks::TaskFiber::State::term) {
        fiber.call();
        bool result = false;
        synchronizedMutex(gcMutex, [&] { result = canceled; });
        if (result) {
            if (frame->currentTarget) setStatus(nodeUuid(frame->currentTarget), Status::Failed);
            break;
        }
    }
    // C++ resources are released by their ordinary owners; no collection phase.
}

void AutoMeshBatchWindow::onBeginUpdate() {
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const auto positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const auto positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    const ImVec2 position{positionX + halfWidth, positionY + halfHeight};
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({800, 600}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({800, 600}, {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});
    Modal::onBeginUpdate();
}

void AutoMeshBatchWindow::onUpdate() {
    const auto space = ngAvailableSpace();
    const float gap = 8, width = space.x / 3 - gap, height = std::floor(space.y - 28 - 6);
    const float preview = height < width ? height : width;
    ImGui::BeginGroup();
    if (ImGui::BeginChild("###Nodes", {width, height})) {
        if (ngCheckbox("##toggleCheck", &selectAll)) toggleAction = selectAll ? ToggleAction::All : ToggleAction::None;
        ImGui::SameLine(0, 0); ngInputText("##", width, nodeFilter);
        ImGui::BeginListBox("###NodeList", {width, height - 26}); treeView(); ImGui::EndListBox();
    }
    ImGui::EndChild(); ImGui::SameLine(0, gap); ImGui::BeginGroup();
    ImGui::BeginChild("###Processors", {width, 40});
    auto processors = viewport::vertex::ngAutoMeshProcessors();
    for (auto processor : processors) {
        const auto icon = compat::ngRequireObject(processor)->icon();
        const compat::SourceCString terminatedIcon{compat::RetainedStringSlice(icon)};
        compat::detail::ngSourceCStringPostconditions(icon.view(), terminatedIcon.data());
        if (ngButtonColored(terminatedIcon.data(), {0, 0}, processor == viewport::vertex::ngActiveAutoMeshProcessor()
                ? colorUndefined : ImVec4(.6f, .6f, .6f, 1))) viewport::vertex::ngActiveAutoMeshProcessor(processor);
        ImGui::SameLine(0, 2);
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("##Configs", {width, height - 80}))
        compat::ngRequireObject(viewport::vertex::ngActiveAutoMeshProcessor())->configure();
    ImGui::EndChild();
    if (processingThread && !processingThread->running.load(std::memory_order_acquire)) {
        processingThread->join(); processingThread.reset();
        auto retained = lockedParts;
        for (auto part : retained) compat::ngRequireObject(compat::ngRequireObject(part)->textures[0])->unlock();
        lockedParts.resize(0); canceled = false;
        // C++ resources are released by their ordinary owners; no collection phase.
        if (closeAfterProcessing) { closeAfterProcessing = false; close(); return; }
    }
    ImGui::BeginChild("###Actions", {width, 40});
    if (ngButtonColored(core::translation::ngTranslateCString(
            processingThread ? "Cancel" : "Run batch").data())) {
        if (!processingThread) {
            compat::Slice<std::shared_ptr<Part>> parts;
            auto retained = nodes;
            for (auto node : retained) {
                const auto presenceKey = nodeUuid(node);
                const auto presenceMap = selected;
                if (!presenceMap.contains(presenceKey)) continue;
                // The original predicate reads uuid again for AA indexing.
                // Retain that receiver before the virtual getter can reenter.
                const auto selectedMap = selected;
                const auto selectedKey = nodeUuid(node);
                const auto found = selectedMap.find(selectedKey);
                if (found == selectedMap.end()) {
                    throw compat::ngStaticError<compat::RangeError>(compat::RetainedString::fromStaticLiteral(
                        "nijigenerate/windows/automeshbatch.d"), 412);
                }
                if (found->second && isApplicable(node))
                    parts.append(std::dynamic_pointer_cast<Part>(node));
            }
            lockedParts = parts;
            auto partsToLock = lockedParts;
            for (auto part : partsToLock)
                compat::ngRequireObject(compat::ngRequireObject(part)->textures[0])->lock();
            compat::Slice<std::shared_ptr<Deformable>> targets;
            // lock() may invoke user code; source map reads the current field.
            auto currentParts = lockedParts;
            for (auto part : currentParts)
                if (auto deformable = std::dynamic_pointer_cast<Deformable>(part)) targets.append(deformable);
            compat::Slice<MeshRef> meshList;
            for (auto target : targets)
                meshList.append(IncMesh::create(getMesh(nodeUuid(target))));
            auto processor = viewport::vertex::ngActiveAutoMeshProcessor();
            clearStatuses();
            for (auto target : targets) setStatus(nodeUuid(target), Status::Waiting);
            activeProcessor = processor;
            auto self = shared_from_this();
            auto thread = std::allocate_shared<ProcessingThread>(
                nicxlive::core::d_runtime::SourceAllocator<ProcessingThread>{});
            processingThread = thread;
            thread->running.store(true, std::memory_order_release);
            try {
                thread->worker = std::thread([self, thread, targets, meshList, processor] {
                    try {
                        core::thread_context::ManagedThreadScope managedThread;
                        utils::crashdump::installNativeCrashDumpThreadHandler();
                        self->runBatch(targets, meshList, processor);
                    } catch (...) { thread->failure = std::current_exception(); }
                    thread->running.store(false, std::memory_order_release);
                });
            } catch (const std::system_error&) {
                thread->running.store(false, std::memory_order_release);
                compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
            } catch (...) {
                // A failed start leaves an unstarted handle, not a running worker.
                thread->running.store(false, std::memory_order_release);
                throw;
            }
        } else { synchronizedMutex(gcMutex, [&] { canceled = true; }); }
    }
    ImGui::EndChild(); ImGui::EndGroup(); ImGui::SameLine(0, gap); ImGui::BeginGroup();
    ngDummy({width, (height - preview) / 2});
    if (ImGui::BeginChild("###Preview", {preview, preview}, ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (active) if (auto part = std::dynamic_pointer_cast<Part>(active))
            previewImage(part, {preview / 2, preview / 2}, preview - gap * 2,
                {0, 0}, {1, 1}, {1, 1, 1, 1}, getMesh(nodeUuid(part)));
    }
    ImGui::EndChild(); ImGui::EndGroup(); ImGui::EndGroup(); ImGui::BeginGroup();
    ngDummy({-192, 0}); ImGui::SameLine(0, 0);
    if (ngButtonColored(core::translation::ngTranslateCString("Cancel").data(), {96, 24})) {
        if (isProcessing()) {
            synchronizedMutex(gcMutex, [&] { canceled = true; });
            closeAfterProcessing = true;
        }
        else { canceled = true; close(); }
        ImGui::EndGroup(); return;
    }
    ImGui::SameLine(0, 0); ImGui::BeginDisabled(isProcessing());
    const bool save = ngButtonColored(core::translation::ngTranslateCString("Save").data(), {96, 24});
    ImGui::EndDisabled();
    if (save) { apply(); canceled = true; close(); ImGui::EndGroup(); return; }
    ImGui::EndGroup();
}
void AutoMeshBatchWindow::close() {
    if (processingThread) {
        synchronizedMutex(gcMutex, [&] { canceled = true; });
        closeAfterProcessing = true;
        return;
    }
    widgets::modal::ngModalCloseTop();
}
} // namespace nijigenerate::windows::automeshbatch
