#pragma once
#include "clipboard_state.hpp"
#include "conversion_map.hpp"
#include <nodes/node.hpp>
namespace nijigenerate::commands::node::base {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using Nodes = compat::Slice<NodeRef>;
void copyToClipboard(Nodes nodes);
void pasteFromClipboard(NodeRef parent);
Nodes insertNodesAux(Nodes parents, Nodes children, compat::RetainedString className, compat::RetainedString suffixName);
void incReloadNode(Nodes nodes);
Nodes ngAddNodes(Nodes parents, compat::RetainedString className, compat::RetainedString suffix = {});
Nodes ngInsertNodes(Nodes children, compat::RetainedString className, compat::RetainedString suffix = {});
compat::RetainedString ngGetCommonNodeType(Nodes nodes);
Nodes ngConvertTo(Nodes nodes, compat::RetainedString toType);
}
