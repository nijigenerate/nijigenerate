#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/document.hpp"
#include <span>
namespace nijigenerate::io::psd {
inline constexpr std::uint64_t MaxPsdDecodedPixels = 100000000;
inline constexpr std::uint32_t MaxPsdDecodedLayerChannels = 6;
inline constexpr std::uint64_t MaxPsdDecodedLayerChannelBytes = MaxPsdDecodedPixels * 4;
inline constexpr std::uint64_t MaxPsdEncodedLayerChannelBytes = MaxPsdDecodedLayerChannelBytes;
inline constexpr std::uint64_t PsdEncodedLayerChannelFixedOverhead = 64 * 1024;
bool validPsdImageDimensions(std::int64_t width, std::int64_t height);
bool decodePsdLayerCount(std::int16_t encodedCount, std::uint32_t layerLength,
    std::uint32_t& layerCount, bool& hasTransparencyMask);
bool decodedPsdLayerChannel(std::int16_t channelType);
bool reservePsdDecodedLayerChannel(std::uint64_t length, std::uint32_t& channels, std::uint64_t& bytes);
bool validPsdEncodedChannelLength(std::uint64_t encodedLength, std::uint64_t decodedLength,
    std::uint64_t rowTableLength = 0);
bool validPsdChannelPayloadRange(std::uint64_t offset, std::uint64_t length, std::uint64_t fileLength);
bool validPsdUnicodeLayerNameLength(std::uint32_t blockLength, std::uint32_t characterCount);
std::uint32_t findChannel(Layer* layer, ChannelType type);
std::uint8_t applyMaskSettings(std::uint8_t mask, std::uint8_t density, bool disabled, bool invert);
compat::Slice<std::uint8_t> applyMaskFeather(std::span<const std::uint8_t> data,
    std::int32_t width, std::int32_t height, double feather, std::uint8_t defaultColor);
std::uint8_t sampleMaskAt(Layer& layer, std::int16_t channelType,
    std::span<const std::uint8_t> data, std::size_t pixelIndex);
void extractLayer(Layer& layer);
} // namespace nijigenerate::io::psd
