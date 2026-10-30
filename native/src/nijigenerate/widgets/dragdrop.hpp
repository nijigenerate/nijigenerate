#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <memory>
#include <imgui.h>
namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::widgets {
using DragdropNodeHandle=std::shared_ptr<::nicxlive::core::nodes::Node>;
void ngDragdropNodeList(DragdropNodeHandle node);
void ngDragdropNodeList(compat::Slice<DragdropNodeHandle> nodes);
void ngBeginDragDropFake();
void ngEndDragDropFake();
bool ngHasDragDrop(const char* name);
void ngSetNodeDragDropPayload(DragdropNodeHandle node, ImGuiCond condition = ImGuiCond_Always);
DragdropNodeHandle ngNodeFromDragDropPayload(const ImGuiPayload* payload);
namespace dragdrop {
using ::nijigenerate::widgets::DragdropNodeHandle;
using ::nijigenerate::widgets::ngDragdropNodeList;
using ::nijigenerate::widgets::ngBeginDragDropFake;
using ::nijigenerate::widgets::ngEndDragDropFake;
using ::nijigenerate::widgets::ngHasDragDrop;
}
}
