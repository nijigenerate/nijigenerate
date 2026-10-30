#pragma once
// Selected Vibe ChunkedOutputStream/StreamOutputRange (MIT).
// Copyright 2012-2016 Sönke Ludwig. See port/licenses/mcp-http-transport.txt.
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nijigenerate::api::mcp::http_transport::detail {
template<class Sink> concept SourceChunkSink = requires(Sink& sink, std::span<const std::uint8_t> bytes) {
    { sink.writeExact(bytes) } -> std::same_as<void>;
    { sink.flush() } -> std::same_as<void>;
};
enum class SourceChunkWriteMode { all, once, immediate };

namespace chunk_writer_source {
inline void assertion(bool condition, std::size_t line) {
#ifndef NDEBUG
    if (!condition) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("vibe/http/common.d"), line);
#else
    (void)condition; (void)line;
#endif
}
inline std::span<const std::uint8_t> bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

// AllocAppender's full allocated extent and current data are different slices.
// Reuse resets only fill; reserve asks for additional free bytes, not total size.
class ByteAppender {
    compat::Slice<std::uint8_t> allocation_;
    std::size_t fill_ = 0;
public:
    std::size_t size() const noexcept { return fill_; }
    compat::Slice<std::uint8_t> data() const { return allocation_.slice(0, fill_); }
    void reserve(std::size_t additional) {
        if (allocation_.size() - fill_ < additional) {
            compat::Slice<std::uint8_t> next(std::vector<std::uint8_t>(fill_ + additional));
            if (fill_) std::copy_n(allocation_.data(), fill_, next.data());
            allocation_ = std::move(next);
        }
    }
    void append(std::span<const std::uint8_t> input) {
        reserve(input.size());
        std::copy(input.begin(), input.end(), allocation_.data() + fill_);
        fill_ += input.size();
    }
    void reuseData() noexcept { fill_ = 0; }
    void freeData() { allocation_.clear(); fill_ = 0; }
};

template<SourceChunkSink Sink, std::size_t BufferSize = 256> class StreamOutputRange {
    Sink& stream_;
    std::size_t fill_ = 0;
    std::array<std::uint8_t, BufferSize> data_;
    bool flushInDestructor_ = true;
    void writeToStream(std::span<const std::uint8_t> bytes) {
        // A failed real write suppresses another write during scope destruction.
        flushInDestructor_ = false;
        stream_.writeExact(bytes);
        flushInDestructor_ = true;
    }
public:
    explicit StreamOutputRange(Sink& stream) : stream_(stream) {}
    StreamOutputRange(const StreamOutputRange&) = delete;
    void flush() {
        if (!fill_) return;
        writeToStream(std::span(data_).first(fill_));
        fill_ = 0;
    }
    void put(std::uint8_t byte) {
        data_[fill_++] = byte;
        if (fill_ >= data_.size()) flush();
    }
    void put(std::span<const std::uint8_t> input) {
        if (input.size() + fill_ >= data_.size() * 2) {
            flush();
            writeToStream(input);
            return;
        }
        while (!input.empty()) {
            const auto count = (std::min)(data_.size() - fill_, input.size());
            std::copy_n(input.data(), count, data_.data() + fill_);
            fill_ += count;
            input = input.subspan(count);
            if (fill_ >= data_.size()) flush();
        }
    }
    void put(std::string_view input) { put(bytes(input)); }
    // Invoke at scope exit on success AND failure. Explicit cleanup prevents
    // C++ terminate when a D destructor write throws during another exception.
    void finishScope() { if (flushInDestructor_) flush(); }
};
template<SourceChunkSink Sink> using StreamOutputRange256 = StreamOutputRange<Sink, 256>;
} // namespace chunk_writer_source

// Sink is the real retained proxy/adapter chosen by the owning Connection.
// This object has class identity; copying the writer state is not supported.
template<SourceChunkSink Sink> class SourceChunkedOutputStream final {
public:
    using ChunkExtensionCallback = compat::SharedDelegate<compat::RetainedString(std::span<const std::uint8_t>)>;
private:
    Sink output_;
    chunk_writer_source::ByteAppender buffer_;
    std::size_t maximum_ = 4 * 1024;
    bool finalized_ = false;
    ChunkExtensionCallback extension_;

    static void prepareClassInfo() {
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SourceChunkedOutputStream),
                compat::RetainedString::fromStaticLiteral("vibe.http.common.ChunkedOutputStream"));
            return true;
        }();
        (void)registered;
    }

    void writeChunk(compat::Slice<std::uint8_t> data) {
        chunk_writer_source::StreamOutputRange256<Sink> range(output_);
        compat::ngRunWithScopeExit([&] {
            std::array<char, 2 * sizeof(std::size_t)> number;
            auto remaining = data.size();
            auto cursor = number.size();
            do {
                number[--cursor] = "0123456789abcdef"[remaining & 15];
                remaining >>= 4;
            } while (remaining);
            range.put(std::string_view(number.data() + cursor, number.size() - cursor));
            if (extension_) {
                range.put(std::uint8_t{';'});
                const auto text = extension_({data.data(), data.size()});
                chunk_writer_source::assertion(!text.view().starts_with(';'), 603);
#ifndef NDEBUG
                chunk_writer_source::assertion(text.view().find('\r') == std::string_view::npos, 604);
                chunk_writer_source::assertion(text.view().find('\n') == std::string_view::npos, 605);
#endif
                range.put(text.view());
            }
            range.put("\r\n");
            range.put(std::span<const std::uint8_t>(data.data(), data.size()));
            range.put("\r\n");
        }, [&] { range.finishScope(); });
    }
public:
    explicit SourceChunkedOutputStream(Sink output) : output_(std::move(output)) {}
    SourceChunkedOutputStream(const SourceChunkedOutputStream&) = delete;
    SourceChunkedOutputStream& operator=(const SourceChunkedOutputStream&) = delete;
    std::size_t maxBufferSize() const noexcept { return maximum_; }
    void maxBufferSize(std::size_t bytes) {
        maximum_ = bytes;
        if (buffer_.size() >= maximum_) flush();
    }
    ChunkExtensionCallback chunkExtensionCallback() const { return extension_; }
    void chunkExtensionCallback(ChunkExtensionCallback callback) { extension_ = std::move(callback); }
    std::size_t write(std::span<const std::uint8_t> input, SourceChunkWriteMode mode = SourceChunkWriteMode::all) {
        chunk_writer_source::assertion(!finalized_, 555);
        std::size_t written = 0;
        while (!input.empty()) {
            auto count = input.size();
            if (maximum_ > 0 && maximum_ < buffer_.size() + count)
                count = maximum_ - (std::min)(buffer_.size(), maximum_);
            if (count) {
                buffer_.append(input.first(count));
                input = input.subspan(count);
                written += count;
            }
            if (mode == SourceChunkWriteMode::immediate) break;
            if (mode == SourceChunkWriteMode::once && written > 0) break;
            if (!input.empty()) flush();
        }
        return written;
    }
    std::size_t write(std::string_view input, SourceChunkWriteMode mode = SourceChunkWriteMode::all) {
        return write(chunk_writer_source::bytes(input), mode);
    }
    void flush() {
        chunk_writer_source::assertion(!finalized_, 575);
        const auto data = buffer_.data();
        if (!data.empty()) writeChunk(data);
        output_.flush();
        buffer_.reuseData();
    }
    void finalize() {
        if (finalized_) return;
        flush();
        buffer_.freeData();
        finalized_ = true;
        writeChunk({});
        output_.flush();
    }
    // Inherited object.Object defaults (Digital Mars 2000-2011, BSL-1.0).
    // See nicxlive/core/licenses/druntime-associative-array.txt.
    virtual ~SourceChunkedOutputStream() = default;
    virtual compat::RetainedString toString() {
        prepareClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() noexcept {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<SourceChunkedOutputStream> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<SourceChunkedOutputStream>) {
        prepareClassInfo();
        std::string message = "need opCmp for class ";
        message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
        throw compat::ThrowableException(compat::RetainedString(std::move(message)),
            compat::RetainedString::fromStaticLiteral("object.d"), 195);
    }
};
} // namespace nijigenerate::api::mcp::http_transport::detail
