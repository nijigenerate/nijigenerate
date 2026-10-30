#pragma once

#include "cv/distancetransform.hpp"
#include "cv/contours.hpp"
#include "cv/image.hpp"

namespace nijigenerate::core::cv {

using distancetransform::floatINF;
using distancetransform::dt1d;
using distancetransform::convertToRCArray;
using distancetransform::distanceTransform;
using distancetransform::Distance1dResult;
using distancetransform::DistanceView;
using distancetransform::NearestView;

using contours::RetrievalMode;
using contours::ApproximationMethod;
using contours::ContourHierarchy;
using contours::BoundingBox;
using contours::inBounds;
using contours::DIRECTIONS;
using contours::DIR_MAP;
using contours::suzukiAbeContour;
using contours::fillContourInterior;
using contours::pointInPolygon;
using contours::buildHierarchy;
using contours::ramerDouglasPeucker;
using contours::distanceToSegment;
using contours::approximateContour;
using contours::findContours;
using contours::Vec2i;
using contours::Contour;
using contours::Contours;
using contours::Labels;

using image::ImageFormat;
using image::imageFormatChannelCount;
using image::BitDepth;
using image::Image;
using image::ImageHandle;
using image::ngCreateImage;

} // namespace nijigenerate::core::cv
