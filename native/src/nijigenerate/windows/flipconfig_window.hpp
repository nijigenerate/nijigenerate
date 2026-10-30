#pragma once
#include "flipconfig.hpp"
#include "base.hpp"
#include <nijigenerate/widgets/inputtext.hpp>
#include <nodes/part.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
namespace nijigenerate::windows::flipconfig {
class FlipPairWindow : public base::Window {
    template<class T, class... A> friend std::shared_ptr<T> base::ngCreateWindow(A&&...);
    using Node = nicxlive::core::nodes::Node;
    using Part = nicxlive::core::nodes::Part;
    using Pairs = compat::Slice<std::shared_ptr<FlipPair>>;
    compat::RetainedStringSlice nodeFilter, part1Pattern, part2Pattern;
    static constexpr float PreviewSize = 128;
    compat::Slice<std::shared_ptr<Node>> nodes;
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint32_t, std::uint64_t,
        UuidHash, std::equal_to<std::uint32_t>> map;
    Pairs pairs, activeOwner;
    std::size_t activeIndex{};
    bool hasActive = false;
    void initialize();
    void apply();
    void autoPair(compat::RetainedStringSlice part1, compat::RetainedStringSlice part2);
    nicxlive::core::math::Vec4 previewImage(std::shared_ptr<Part>, ImVec2, float,
        ImVec2 uv0 = {0, 0}, ImVec2 uv1 = {1, 1}, ImVec4 tint = {1, 1, 1, 1});
    void treeView();
    void pairView();
    void assignPair(std::shared_ptr<FlipPair>&, std::shared_ptr<Node>, std::size_t, int);
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
    void onClose() override;
public:
    FlipPairWindow() = default;
    ~FlipPairWindow() override = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<FlipPairWindow> other);
};
bool ngFlipPairWindowObjectEquals(std::shared_ptr<FlipPairWindow> left, std::shared_ptr<FlipPairWindow> right);
}
