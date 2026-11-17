#pragma once

#include <nijigenerate/widgets/output_tree.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace nijigenerate::panels::autorig::detail {

struct EvidenceResourceGroup {
    std::string key;
    compat::RetainedString label;
    std::vector<std::uint32_t> nodes;
    bool operator==(const EvidenceResourceGroup&) const = default;
};

// The editor's UUID index gives result stores access to current resources.
// Scan only when the model tree changes, independently of the evidence shown.
class EvidenceNodeIndex {
    using Node = core::selector::Node;
    std::weak_ptr<Node> root_;
    std::unordered_map<std::uint32_t, std::weak_ptr<Node>> nodes_;
    std::uint64_t revision_ = 0;
    std::shared_ptr<std::atomic_bool> dirty_ = std::make_shared<std::atomic_bool>(true);
    nicxlive::core::nodes::NodeNotifyListener listener_{[dirty = dirty_](const auto&, auto reason) {
        if (reason == nicxlive::core::nodes::NotifyReason::StructureChanged) dirty->store(true);
    }};

public:
    ~EvidenceNodeIndex() {
        if (const auto root = root_.lock()) root->removeNotifyListener(listener_);
    }
    void refresh(const std::shared_ptr<Node>& root) {
        if (const auto previous = root_.lock(); previous != root) {
            if (previous) previous->removeNotifyListener(listener_);
            root_ = root;
            if (root) root->addNotifyListener(listener_);
            dirty_->store(true);
        }
        if (!dirty_->exchange(false)) return;
        ++revision_;
        nodes_.clear();
        if (!root) return;
        std::vector<std::shared_ptr<Node>> pending{root};
        while (!pending.empty()) {
            auto node = std::move(pending.back());
            pending.pop_back();
            nodes_.emplace(node->getUUID(), node);
            for (auto child : node->childrenList()) if (child) pending.push_back(std::move(child));
        }
    }
    std::shared_ptr<Node> find(std::uint32_t id) const {
        const auto found = nodes_.find(id);
        return found == nodes_.end() ? nullptr : found->second.lock();
    }
    std::uint64_t revision() const { return revision_; }
};

class EvidenceBranch final : public core::selector::Resource {
    std::uint32_t id_;
    core::selector::ResourceString label_;
public:
    EvidenceBranch(std::uint32_t id, const compat::RetainedString& label)
        : id_(id), label_(label.toNullableString()) {}
    core::selector::ResourceString name() override { return label_; }
    std::uint32_t uuid() override { return id_; }
    core::selector::ResourceString typeId() override { return {}; }
};

class EvidenceResourceStore final : public core::selector::TreeStore {
public:
    // The evidence owns grouping and order. Populate explicit rows and edges
    // directly; TreeStore::setResources infers ancestry and is not used here.
    void assign(const std::vector<EvidenceResourceGroup>& groups, const EvidenceNodeIndex& index,
            const compat::RetainedString& unavailable) {
        reset();
        std::unordered_map<std::uint32_t, core::selector::ResourcePtr> leaves;
        std::vector<core::selector::ResourcePtr> allRows;
        std::vector<core::selector::ResourcePtr> topRows;
        for (const auto& group : groups) for (const auto id : group.nodes) {
            if (leaves.contains(id)) continue;
            auto node = index.find(id);
            core::selector::ResourcePtr leaf;
            if (node) leaf = std::make_shared<core::selector::Proxy<core::selector::Node>>(std::move(node));
            else leaf = std::make_shared<EvidenceBranch>(id, unavailable);
            nodeIncluded[leaf] = true;
            allRows.push_back(leaf);
            leaves.emplace(id, std::move(leaf));
        }
        std::unordered_set<std::uint32_t> branchIds;
        for (const auto& group : groups) {
            std::vector<core::selector::ResourcePtr> members;
            std::unordered_set<std::uint32_t> included;
            for (const auto id : group.nodes) {
                const auto found = leaves.find(id);
                if (found != leaves.end() && included.insert(id).second) members.push_back(found->second);
            }
            if (members.empty()) continue;
            if (group.key.empty()) {
                topRows.insert(topRows.end(), members.begin(), members.end());
                continue;
            }
            auto id = ImHashStr(group.key.data(), group.key.size());
            while (id == 0 || leaves.contains(id) || !branchIds.insert(id).second) ++id;
            const auto branch = std::make_shared<EvidenceBranch>(id, group.label);
            topRows.push_back(branch);
            children[branch] = core::selector::SelectorArray<core::selector::ResourcePtr>(std::move(members));
        }
        nodes = core::selector::SelectorArray<core::selector::ResourcePtr>(std::move(allRows));
        roots = core::selector::SelectorArray<core::selector::ResourcePtr>(std::move(topRows));
    }
};

class EvidenceResourceOutput final : public widgets::output::IconTreeOutput {
    const std::function<void(std::uint32_t)>* editNode_ = nullptr;

    ImGuiTreeNodeFlags setFlag(core::selector::ResourcePtr item) override {
        auto flags = IconTreeOutput::setFlag(item);
        if (roots().size() > 1 && std::dynamic_pointer_cast<EvidenceBranch>(item))
            flags &= ~ImGuiTreeNodeFlags_DefaultOpen;
        return flags;
    }
    void drawTreeItem(core::selector::ResourcePtr item) override {
        if (const auto branch = std::dynamic_pointer_cast<EvidenceBranch>(item)) {
            const auto label = branch->name().view();
            ImGui::SameLine(0, 0);
            if (children().contains(item))
                ImGui::TextWrapped("%s  %.*s (%zu)", "\ue867", static_cast<int>(label.size()), label.data(),
                    children().at(item).size()); // label
            else ImGui::TextDisabled("%.*s", static_cast<int>(label.size()), label.data());
            return;
        }
        if (editNode_ && *editNode_) {
            ImGui::SameLine(0, 0);
            (*editNode_)(item->uuid());
        }
        IconTreeOutput::drawTreeItem(std::move(item));
    }
public:
    explicit EvidenceResourceOutput(std::shared_ptr<EvidenceResourceStore> store)
        : IconTreeOutput(std::move(store), nullptr, true) {}
    void render(const std::function<void(std::uint32_t)>& editNode) {
        editNode_ = &editNode;
        compat::ngRunWithScopeExit([&] { onUpdate(); }, [&] { editNode_ = nullptr; });
    }
};

} // namespace nijigenerate::panels::autorig::detail
