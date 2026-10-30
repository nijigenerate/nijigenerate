#pragma once
#include <cstdint>

namespace nicxlive::core {
struct Animation;
struct AnimationLane;
}

namespace nijigenerate::widgets::timeline {
inline constexpr std::int32_t minTrackHeight = 26;
inline constexpr std::int32_t minHeaderWidth = 128;
inline constexpr std::int32_t maxHeaderWidth = minHeaderWidth * 3;
inline constexpr double defaultHeaderWidth = maxHeaderWidth * 0.75;
inline constexpr std::int32_t dragSize = 4;
inline constexpr std::int32_t keyframeSize = 6;
inline constexpr double timelineMinZoom = 0.5;
inline constexpr std::int32_t timelineMaxZoom = 10;
inline constexpr double frameWidth = 16.0;

void ngAnimationLaneHeader(nicxlive::core::AnimationLane& lane, float& width, float& height);
void ngHeaderResizer(float& width, bool side = false);
void ngBeginTimelinePlayhead(nicxlive::core::Animation& animation, float zoom);
void ngEndTimelinePlayhead(nicxlive::core::Animation& animation, float zoom, float frame);
void ngTimelineLane(nicxlive::core::AnimationLane& lane, nicxlive::core::Animation& animation,
                    float zoom, std::int32_t index, float* hoveredFrame, float* hoveredValue);
} // namespace nijigenerate::widgets::timeline
