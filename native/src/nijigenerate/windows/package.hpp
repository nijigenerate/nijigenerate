#pragma once

/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/

#include "base.hpp"
#include "about.hpp"
// Settings remains an explicit import at each consumer, matching package.d.
#include "texviewer.hpp"
#include "parameditor.hpp"
#include "paramsplit.hpp"
#include "psdmerge.hpp"
#include "psddepthmap.hpp"
#include "kramerge.hpp"
#include "welcome.hpp"
#include "rename.hpp"
#include "imgexport.hpp"
#include "flipconfig.hpp"
#include "flipconfig_window.hpp"
#include "videoexport.hpp"
#include "editanim.hpp"
#include "automeshbatch.hpp"
#include "command_browser.hpp"

namespace nijigenerate::windows {

// Public imports preserve each actual module namespace and its original types.
// No new window instances, startup hooks or duplicate class definitions exist here.
using namespace base;
using namespace about;
using namespace texviewer;
using namespace parameditor;
using namespace paramsplit;
using namespace psdmerge;
using namespace psddepthmap;
using namespace kramerge;
using namespace welcome;
using namespace rename;
using namespace imgexport;
using namespace flipconfig;
using namespace videoexport;
using namespace editanim;
using namespace automeshbatch;
using namespace command_browser;

} // namespace nijigenerate::windows
