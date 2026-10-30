#pragma once
#include "string.hpp"
#include "apply.hpp"
#include "autobind.hpp"
#include "binding.hpp"
#include "composer.hpp"
#include "coordinate.hpp"
#include "diagnostics.hpp"
#include "fitgap.hpp"
#include "gpu.hpp"
#include "layer.hpp"
#include "manifest.hpp"
#include "persistence.hpp"
#include "pngexport.hpp"
#include "readback.hpp"
#include "session.hpp"
#include "source.hpp"
#include "viewport.hpp"

namespace nijigenerate::viewport::depth::draw {
// Re-export the actual coordinate functions, preserving their provider identity.
using coordinate::ngDepthDrawLayerDocumentBounds;
using coordinate::ngDepthDrawVertexDocumentPosition;
using coordinate::ngDepthDrawLayerPixelFromDocument;
using coordinate::ngDepthDrawLayerPixelFromVertex;
}
