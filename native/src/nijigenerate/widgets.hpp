#pragma once

// Public imports of nijigenerate.widgets, preserving canonical provider ownership.
#include <imgui.h>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/widgets/progress.hpp>
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/widgets/toolbar.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/lock.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/widgets/timeline.hpp>
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/widgets/modal.hpp>
#include <nijigenerate/widgets/toggle.hpp>
#include <nijigenerate/widgets/window.hpp>

namespace nijigenerate::widgets {
using dialog::DialogLevel;
using dialog::DialogButtons;
using dialog::DialogHandler;
using dialog::ngInitDialogs;
using dialog::ngRenderDialogs;
using dialog::ngDialog;
using dialog::ngCleanupDialogs;
using dialog::ngDialogButtonSelected;
using dialog::ngDialogButtonUserData;
using dialog::ngRegisterDialogHandler;
using dialog::ngHandleDialogHandlers;

using timeline::minTrackHeight;
using timeline::minHeaderWidth;
using timeline::maxHeaderWidth;
using timeline::defaultHeaderWidth;
using timeline::dragSize;
using timeline::keyframeSize;
using timeline::timelineMinZoom;
using timeline::timelineMaxZoom;
using timeline::frameWidth;
using timeline::ngAnimationLaneHeader;
using timeline::ngHeaderResizer;
using timeline::ngBeginTimelinePlayhead;
using timeline::ngEndTimelinePlayhead;
using timeline::ngTimelineLane;

using modal::Modal;
using modal::Nagscreen;
using modal::ngModalRender;
using modal::ngModalIsOpen;
using modal::ngModalAdd;
using modal::ngModalCloseTop;
}
