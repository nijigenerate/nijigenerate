#pragma once

#include <span>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <tuple>
#include <type_traits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/source_const_slice_identity.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/compat/byte_view.hpp>
#include <nijigenerate/compat/contiguous_view.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <typeinfo>

namespace nijigenerate::core::cv::image {

template<class T> concept ImageDataElement = std::same_as<T, std::uint8_t> ||
    std::same_as<T, std::uint16_t> || std::same_as<T, float>;

class Image;
using ImageHandle = std::shared_ptr<Image>;

enum class ImageFormat : std::int32_t {
    IF_UNASSIGNED = 0,
    IF_MONO,
    IF_RGB,
    IF_RGB_ALPHA,
};

namespace detail {
class ImmutableImageChannelView {
public:
    constexpr explicit ImmutableImageChannelView(std::span<const std::size_t> values) : values_(values) {}
    constexpr ImmutableImageChannelView(const ImmutableImageChannelView&) = default;
    ImmutableImageChannelView& operator=(const ImmutableImageChannelView&) = delete;
    constexpr const std::size_t* data() const noexcept { return values_.data(); }
    constexpr std::size_t size() const noexcept { return values_.size(); }
    constexpr const std::size_t& operator[](std::size_t index) const {
        if (index >= values_.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values_.size(), std::source_location::current());
        return values_[index];
    }
    constexpr auto begin() const noexcept { return values_.begin(); }
    constexpr auto end() const noexcept { return values_.end(); }
    constexpr std::span<const std::size_t> subspan(std::size_t offset, std::size_t count) const {
        return values_.subspan(offset, count);
    }
private:
    const std::span<const std::size_t> values_;
};
inline constexpr std::size_t imageFormatChannelStorage[]{0, 1, 3, 4};
}
inline constexpr detail::ImmutableImageChannelView imageFormatChannelCount{detail::imageFormatChannelStorage};

enum class BitDepth : std::size_t {
    BD_UNASSIGNED = 0,
    BD_8 = 8,
};

class Image : public std::enable_shared_from_this<Image> {
public:
    // Source constructors run after the most-derived native object exists.
    struct DeferredInitialization {};
    explicit Image(DeferredInitialization) { prepareClassInfo(); }
    Image(std::size_t width, std::size_t height, ImageFormat format) {
        initialize(width, height, format);
    }
    void initialize(std::size_t width, std::size_t height, ImageFormat format) {
        prepareClassInfo();
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        _width = width;
        _height = height;
        _format = format;
        _depth = BitDepth::BD_8;
        const auto count = width * height * channels();
        if (count) {
            try {
                _data = compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(count));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        } else _data = {};
    }
    Image(std::size_t width, std::size_t height, ImageFormat format,
          BitDepth depth, compat::Slice<std::uint8_t> data) {
        initialize(width, height, format, depth, std::move(data));
    }
    void initialize(std::size_t width, std::size_t height, ImageFormat format,
          BitDepth depth, compat::Slice<std::uint8_t> data) {
        prepareClassInfo();
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        _width = width;
        _height = height;
        _format = format;
        _depth = depth;
        if (data.size() != width * height * channels()) {
            static constexpr char8_t message[] = u8"\u30d0\u30c3\u30d5\u30a1\u30b5\u30a4\u30ba\u304c\u753b\u50cf\u306e"
                u8"\u30b5\u30a4\u30ba\u3068\u30c1\u30e3\u30f3\u30cd\u30eb\u6570\u306b\u4e00\u81f4\u3057\u307e\u305b\u3093";
            const auto sourceMessage = compat::RetainedString::fromStaticView(
                std::string_view(reinterpret_cast<const char*>(message), sizeof(message) - 1));
            throw compat::ThrowableException(sourceMessage,
                compat::RetainedString::fromStaticLiteral("nijigenerate/core/cv/image.d"), 54);
        }
        _data = std::move(data);
    }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    virtual ~Image() = default;
    // Inherited object.d defaults; Digital Mars, BSL-1.0.
    // Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
    virtual compat::RetainedString toString() {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        prepareClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<Image> other) {
        return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
    }
    virtual int opCmp(std::shared_ptr<Image>) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        prepareClassInfo();
        const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
        compat::RetainedString message;
        try {
            std::string bytes = "need opCmp for class ";
            bytes += name.view();
            message = compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        throw compat::ThrowableException(std::move(message),
            compat::RetainedString::fromStaticLiteral("object.d"), 195);
    }
    virtual std::size_t width() const { return _width; }
    virtual std::size_t height() const { return _height; }
    virtual ImageFormat format() const { return _format; }
    virtual BitDepth depth() const { return _depth; }
    virtual std::size_t channels() const {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        switch (format()) {
        case ImageFormat::IF_MONO: return 1;
        case ImageFormat::IF_RGB: return 3;
        case ImageFormat::IF_RGB_ALPHA: return 4;
        default: return 0;
        }
    }
    virtual compat::Slice<std::uint64_t> shape() const {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        compat::Slice<std::uint64_t> result;
        try {
            result.resize(3);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        result[0] = _height;
        result[1] = _width;
        result[2] = channels();
        return result;
    }
    template<ImageDataElement T = std::uint8_t> auto data() {
        if constexpr (std::same_as<T, std::uint8_t>) return _data;
        else return compat::ByteView<T>(_data, "inout(ubyte)");
    }
    template<ImageDataElement T = std::uint8_t> auto data() const {
        if constexpr (std::same_as<T, std::uint8_t>) return compat::ConstSlice<std::uint8_t>(_data);
        // The original explicitly casts the other branches to T[], which
        // discards the byte array's inout qualifier. Keep that authored cast.
        else return compat::ByteView<T>(_data, "inout(ubyte)");
    }
    virtual bool empty() const { return _data.isNull(); }
    template<ImageDataElement T = std::uint8_t> auto sliced() { return slicedImpl<T>(*this); }
    template<ImageDataElement T = std::uint8_t> auto sliced() const { return slicedImpl<T>(*this); }
private:
    static void prepareClassInfo() try {
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Image),
                compat::RetainedString::fromStaticLiteral("nijigenerate.core.cv.image.Image"));
            return true;
        }();
        (void)registered;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    template<ImageDataElement T, class Self> static auto slicedImpl(Self& self) {
        [[maybe_unused]] const auto receiver = self.weak_from_this().lock();
        auto typed = self.template data<T>();
        const std::array<std::size_t, 3> lengths{self._height, self._width, self.channels()};
        const auto count = lengths[0] * lengths[1] * lengths[2];
#ifndef NDEBUG
        if (count > typed.size())
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral(
                    "array length should be greater or equal to the product of constructed ndslice lengths"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 253);
#endif
        if constexpr (std::same_as<T, std::uint8_t>) {
            if (typed.empty()) typed = {};
            using Element = std::conditional_t<std::is_const_v<Self>, const T, T>;
            return compat::ContiguousView<Element, 3, decltype(typed)>(std::move(typed), lengths);
        } else {
            if (typed.empty()) typed.clear();
            return compat::ContiguousView<T, 3, compat::ByteView<T>>(std::move(typed), lengths);
        }
    }
    ImageFormat _format = ImageFormat::IF_UNASSIGNED;
    BitDepth _depth = BitDepth::BD_UNASSIGNED;
    std::size_t _width = 0;
    std::size_t _height = 0;
    compat::Slice<std::uint8_t> _data;
};

// Derived source types expose the deferred constructor and lower their source
// constructor through this factory, so channels()/format() dispatch correctly.
namespace detail {
template<class Type> std::shared_ptr<Type> allocateImageReceiver() {
    return std::allocate_shared<Type>(nicxlive::core::d_runtime::SourceAllocator<Type>{},
        Image::DeferredInitialization{});
}
}

template<class Type = Image, class... Args> requires std::derived_from<Type, Image>
std::shared_ptr<Type> ngCreateImage(Args&&... args) {
    // The allocator catches only its memory request. Derived constructors and
    // their user callbacks execute outside that catch, as does initialize below.
    auto image = detail::allocateImageReceiver<Type>();
    image->Image::initialize(std::forward<Args>(args)...);
    return image;
}

template<class Type = Image, class... Producers> requires std::derived_from<Type, Image>
std::shared_ptr<Type> ngCreateImageDeferred(Producers&&... producers) {
    auto image = detail::allocateImageReceiver<Type>();
    auto arguments = std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...>{
        std::invoke(std::forward<Producers>(producers))...};
    std::apply([&](auto&&... values) {
        image->Image::initialize(std::forward<decltype(values)>(values)...);
    }, std::move(arguments));
    return image;
}

} // namespace nijigenerate::core::cv::image

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<cv::image::ImageFormat> {
    using Enum = cv::image::ImageFormat;
    inline static constexpr std::string_view name = "ImageFormat";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::IF_UNASSIGNED, "IF_UNASSIGNED"},
        FormatEnumMember<Enum>{Enum::IF_MONO, "IF_MONO"},
        FormatEnumMember<Enum>{Enum::IF_RGB, "IF_RGB"},
        FormatEnumMember<Enum>{Enum::IF_RGB_ALPHA, "IF_RGB_ALPHA"}};
};
template<> struct FormatEnumTraits<cv::image::BitDepth> {
    using Enum = cv::image::BitDepth;
    inline static constexpr std::string_view name = "BitDepth";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::BD_UNASSIGNED, "BD_UNASSIGNED"},
        FormatEnumMember<Enum>{Enum::BD_8, "BD_8"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {

#define NG_CV_IMAGE_ENUM_SOURCE_NAMES(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::core::cv::image::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.image." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.image." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_CV_IMAGE_ENUM_SOURCE_NAMES(ImageFormat)
NG_CV_IMAGE_ENUM_SOURCE_NAMES(BitDepth)
#undef NG_CV_IMAGE_ENUM_SOURCE_NAMES
template<> struct SourceObjectTypeNames<::nijigenerate::core::cv::image::Image> {
    inline static constexpr char className[] = "nijigenerate.core.cv.image.Image";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.cv.image.Image)";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::cv::image::Image> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.image.Image)";
    inline static constexpr char unqualifiedArgument[] = "(Image)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.image.Image";
    inline static constexpr char unqualifiedElement[] = "Image";
};
}
