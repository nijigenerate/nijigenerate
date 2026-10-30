#pragma once
#include "command_browser_reflection.hpp"
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/viewport/depth/mesheditor/editor.hpp>
#include <nijigenerate/viewport/depth/mesheditor/node.hpp>
#include <nijigenerate/viewport/depth/tools/operation.hpp>

namespace nijigenerate::windows::command_browser::detail {

template<> struct BrowserSourceResultTraits<std::shared_ptr<panels::Panel>> {
    static BrowserText stringify(const std::shared_ptr<panels::Panel>& value) {
        return value ? BrowserText(value->toString()) : staticText("null");
    }
};
template<> struct BrowserSourceResultTraits<nicxlive::core::math::Vec2> {
    static BrowserText stringify(nicxlive::core::math::Vec2 value) {
        return format("[%s, %s]",value.x,value.y);
    }
};
template<> struct BrowserSourceResultTraits<nicxlive::core::math::Vec3> {
    static BrowserText stringify(nicxlive::core::math::Vec3 value) {
        return format("[%s, %s, %s]",value.x,value.y,value.z);
    }
};
template<> struct BrowserSourceResultTraits<nicxlive::core::math::Vec4> {
    static BrowserText stringify(nicxlive::core::math::Vec4 value) {
        return format("[%s, %s, %s, %s]",value.x,value.y,value.z,value.w);
    }
};
template<> struct BrowserSourceResultTraits<std::shared_ptr<ext::ExParameterGroup>> {
    static BrowserText stringify(const std::shared_ptr<ext::ExParameterGroup>& value) {
        return value ? BrowserText(value->toString()) : staticText("null");
    }
};
template<> struct BrowserSourceResultTraits<std::shared_ptr<viewport::depth::mesheditor::editor::DepthMeshEditor>> {
    static BrowserText stringify(const std::shared_ptr<viewport::depth::mesheditor::editor::DepthMeshEditor>& value) {
        return value ? BrowserText(value->toString()) : staticText("null");
    }
};
template<> struct BrowserSourceResultTraits<std::shared_ptr<viewport::depth::mesheditor::node::DepthMeshEditorOne>> {
    static BrowserText stringify(const std::shared_ptr<viewport::depth::mesheditor::node::DepthMeshEditorOne>& value) {
        return value ? BrowserText(value->toString()) : staticText("null");
    }
};
template<> struct BrowserSourceResultTraits<std::shared_ptr<viewport::depth::tools::operation::DepthOperation>> {
    static BrowserText stringify(const std::shared_ptr<viewport::depth::tools::operation::DepthOperation>& value) {
        return value ? BrowserText(value->toString()) : staticText("null");
    }
};
}
