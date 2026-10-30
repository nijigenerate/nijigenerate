#pragma once
#include "base.hpp"
#include "circlebrush.hpp"
#include "doublethreshbrush.hpp"
#include "rectanglebrush.hpp"
#include <memory>

namespace nijigenerate::viewport::common::mesheditor::brushes {
using base::Brush;
using circlebrush::CircleBrush;
using doublethreshbrush::DoubleThreshBrush;
using rectanglebrush::RectangleBrush;

compat::Slice<std::shared_ptr<base::Brush>> ngBrushList();
}
