#pragma once
#include <memory>
namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::panels::nodes {
void ngInitializeNodesPanelModule();
void ngAddOrInsertNodeMenu(bool add);
template<bool add> inline void ngAddOrInsertNodeMenu() { ngAddOrInsertNodeMenu(add); }
inline void ngAddNodeMenu() { ngAddOrInsertNodeMenu<true>(); }
inline void ngInsertNodeMenu() { ngAddOrInsertNodeMenu<false>(); }
void incNodeActionsPopup(const char* title, std::shared_ptr<nicxlive::core::nodes::Node> node,
    bool isRoot = false, bool icon = false);
}
