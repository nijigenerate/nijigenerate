#pragma once
#include <nijigenerate/compat/source_readonly_value.hpp>
#include <nijigenerate/compat/source_json_arrays.hpp>
#include <nijigenerate/compat/source_json_enums.hpp>
#include <nijigenerate/core/typed_format.hpp>

#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/range_error.hpp"
#include "nijigenerate/compat/phobos_unicode_exception.hpp"
#include <common/string_key_map.hpp>
#include <compat/source_allocator.hpp>
#include <compat/phobos_string_hash.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <limits>

#include <cstdint>
#include <cstddef>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace nijigenerate::core::selector {

struct SelectorStringHash {
    using is_transparent = void;
    std::size_t operator()(const nijigenerate::compat::RetainedStringSlice& value) const noexcept {
        return (*this)(value.view());
    }
    std::size_t operator()(std::string_view value) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(value);
    }
};

struct SelectorStringEqual {
    using is_transparent = void;
    bool operator()(const nijigenerate::compat::RetainedStringSlice& left,
                    const nijigenerate::compat::RetainedStringSlice& right) const noexcept {
        return left.view() == right.view();
    }
    bool operator()(const nijigenerate::compat::RetainedStringSlice& left,
                    std::string_view right) const noexcept { return left.view() == right; }
    bool operator()(std::string_view left,
                    const nijigenerate::compat::RetainedStringSlice& right) const noexcept {
        return left == right.view();
    }
};

template<class K, class V, class Hash = std::hash<K>, class Equal = std::equal_to<K>> class SelectorMap {
    using Map = std::unordered_map<K, V, Hash, Equal,
        nicxlive::core::d_runtime::SourceAllocator<std::pair<const K, V>>>;
    std::shared_ptr<Map> storage_;
    static Map& nullMap() { static Map empty; return empty; }
    Map& storage() {
        if (!storage_) storage_ = std::allocate_shared<Map>(
            nicxlive::core::d_runtime::SourceAllocator<Map>{});
        return *storage_;
    }
public:
    SelectorMap() = default;
    SelectorMap(const SelectorMap&) = default;
    SelectorMap& operator=(const SelectorMap&) = default;
    // Native move transport must retain the D associative-array handle at both ends.
    SelectorMap(SelectorMap&& other) noexcept : storage_(other.storage_) {}
    SelectorMap& operator=(SelectorMap&& other) noexcept {
        storage_ = other.storage_;
        return *this;
    }
    bool empty() const { return !storage_ || storage_->empty(); }
    bool isNull() const noexcept { return !storage_; }
    const void* identity() const noexcept { return storage_.get(); }
    void reset() noexcept { storage_.reset(); }
    // D AA assignment shares the handle; explicit .dup copies only its entries.
    SelectorMap duplicate() const {
        SelectorMap result;
        if (storage_ && !storage_->empty()) result.storage_ = std::allocate_shared<Map>(
            nicxlive::core::d_runtime::SourceAllocator<Map>{}, *storage_);
        return result;
    }
    std::size_t size() const { return storage_ ? storage_->size() : 0; }
    void clear() { if (storage_) storage_->clear(); }
    std::size_t erase(const K& key) { return storage_ ? storage_->erase(key) : 0; }
    auto begin() { return storage_ ? storage_->begin() : nullMap().begin(); }
    auto end() { return storage_ ? storage_->end() : nullMap().end(); }
    auto begin() const { return storage_ ? storage_->cbegin() : nullMap().cbegin(); }
    auto end() const { return storage_ ? storage_->cend() : nullMap().cend(); }
    template<class Q> auto find(const Q& key) { return storage_ ? storage_->find(key) : nullMap().find(key); }
    template<class Q> auto find(const Q& key) const {
        return storage_ ? storage_->find(key) : nullMap().cend();
    }
    template<class Q> bool contains(const Q& key) const { return storage_ && storage_->contains(key); }
    V& operator[](const K& key) { return storage()[key]; }
    V& at(const K& key) {
        const auto found = find(key);
        if (found == end()) throw compat::RangeError{};
        return found->second;
    }
    const V& at(const K& key) const {
        const auto found = find(key);
        if (found == end()) throw compat::RangeError{};
        return found->second;
    }
    template<class Q> requires (!std::convertible_to<const Q&, K>)
    V& at(const Q& key) {
        const auto found = find(key);
        if (found == end()) throw compat::RangeError{};
        return found->second;
    }
    template<class Q> requires (!std::convertible_to<const Q&, K>)
    const V& at(const Q& key) const {
        const auto found = find(key);
        if (found == end()) throw compat::RangeError{};
        return found->second;
    }
    template<class... Args> auto try_emplace(const K& key, Args&&... args) {
        return storage().try_emplace(key, std::forward<Args>(args)...);
    }
};

// A copied D slice retains its elements but has an independent length.
template<class T> class SelectorArray {
    std::shared_ptr<std::vector<T>> storage_;
    std::size_t offset_ = 0;
    std::size_t length_ = 0;
    static auto makeStorage() {
        return std::allocate_shared<std::vector<T>>(
            nicxlive::core::d_runtime::SourceAllocator<std::vector<T>>{});
    }
    // Called only on empty storage, so no T copy/move can occur in this catch.
    static void reserveEmpty(std::vector<T>& storage, std::size_t count) {
        try { storage.reserve(count); }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
public:
    SelectorArray() = default;
    SelectorArray(const SelectorArray&) = default;
    SelectorArray& operator=(const SelectorArray&) = default;
    // Moving a native descriptor must not leave a nonzero length with null backing.
    SelectorArray(SelectorArray&& other) noexcept
        : storage_(other.storage_), offset_(other.offset_), length_(other.length_) {}
    SelectorArray& operator=(SelectorArray&& other) noexcept {
        storage_ = other.storage_;
        offset_ = other.offset_;
        length_ = other.length_;
        return *this;
    }
    SelectorArray(std::vector<T> values) : storage_(std::allocate_shared<std::vector<T>>(
        nicxlive::core::d_runtime::SourceAllocator<std::vector<T>>{}, std::move(values))),
        length_(storage_->size()) {}
    explicit SelectorArray(std::shared_ptr<std::vector<T>> values)
        : storage_(std::move(values)), length_(storage_ ? storage_->size() : 0) {}
    auto owner() const { return storage_; }
    SelectorArray(std::initializer_list<T> values) : storage_(makeStorage()) {
        reserveEmpty(*storage_, values.size());
        for (const auto& value : values) storage_->push_back(value);
        length_ = storage_->size();
    }
    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool isNull() const noexcept { return !storage_; }
    T* data() noexcept {
        if (!storage_) return nullptr;
        return offset_ ? storage_->data() + offset_ : storage_->data();
    }
    const T* data() const noexcept {
        if (!storage_) return nullptr;
        return offset_ ? storage_->data() + offset_ : storage_->data();
    }
    SelectorArray slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_) throw compat::ArraySliceError(first, last, length_);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    T& operator[](std::size_t i) { return at(i); }
    const T& operator[](std::size_t i) const { return at(i); }
    T& at(std::size_t i) {
        if (i >= length_) throw compat::ArrayIndexError(i, length_);
        return storage_->at(offset_ + i);
    }
    const T& at(std::size_t i) const {
        if (i >= length_) throw compat::ArrayIndexError(i, length_);
        return storage_->at(offset_ + i);
    }
    auto begin() {
        return storage_ ? storage_->begin() + static_cast<std::ptrdiff_t>(offset_)
                        : typename std::vector<T>::iterator{};
    }
    auto end() { return length_ ? begin() + static_cast<std::ptrdiff_t>(length_) : begin(); }
    auto begin() const {
        return storage_ ? storage_->cbegin() + static_cast<std::ptrdiff_t>(offset_)
                        : typename std::vector<T>::const_iterator{};
    }
    auto end() const { return length_ ? begin() + static_cast<std::ptrdiff_t>(length_) : begin(); }
    void clear() { length_ = 0; }
    template<class... Args> void emplace_back(Args&&... args) { push_back(T(std::forward<Args>(args)...)); }
    void push_back(T value) {
        if (!storage_) storage_ = makeStorage();
        if (offset_ != 0 || length_ != storage_->size() || length_ == storage_->capacity()) {
            if (length_ > (std::numeric_limits<std::size_t>::max() - 1) / 2)
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            auto replacement = makeStorage();
            reserveEmpty(*replacement, length_ * 2 + 1);
            for (const auto& item : *this) replacement->push_back(item);
            storage_ = std::move(replacement);
            offset_ = 0;
        }
        storage_->push_back(std::move(value));
        ++length_;
    }
    // Used class-reference bulk append: allocate the whole extent before publishing it.
    void appendRange(SelectorArray values)
        requires (std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>
            && std::is_nothrow_default_constructible_v<T>) {
        const auto count = values.size();
        if (!count) return;
        if (storage_ && offset_ == 0 && length_ == storage_->size()
                && count <= storage_->capacity() - length_) {
            const auto previousLength = length_;
            storage_->resize(previousLength + count);
            for (std::size_t i = 0; i < count; ++i) (*storage_)[previousLength + i] = values[i];
            length_ = previousLength + count;
            return;
        }
        // Both descriptors keep the real inputs alive, including self/slice aliases.
        const std::array<std::span<const T>, 2> parts{
            std::span<const T>{data(), length_}, std::span<const T>{values.data(), count}};
        const auto joined = parts | std::views::join;
        if (count > std::numeric_limits<std::size_t>::max() - length_)
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        auto replacement = makeStorage();
        reserveEmpty(*replacement, length_ + count);
        for (const auto& item : joined) replacement->push_back(item);
        storage_ = std::move(replacement);
        offset_ = 0;
        length_ = storage_->size();
    }
};


// std::vector<bool> cannot supply the actual addressable bool cells of D bool[].
template<> class SelectorArray<bool> {
    compat::Slice<bool> values_;
public:
    SelectorArray() = default;
    SelectorArray(const SelectorArray&) = default;
    SelectorArray& operator=(const SelectorArray&) = default;
    SelectorArray(SelectorArray&&) noexcept = default;
    SelectorArray& operator=(SelectorArray&&) noexcept = default;
    SelectorArray(std::vector<bool> values) : values_(std::move(values)) {}
    SelectorArray(std::initializer_list<bool> values) : values_(values) {}
    static SelectorArray retainedView(std::shared_ptr<void> owner, bool* values, std::size_t length) {
        SelectorArray result;
        result.values_ = compat::Slice<bool>::retainedView(std::move(owner), values, length);
        return result;
    }
    auto owner() const noexcept { return values_.retentionOwner(); }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
    bool isNull() const noexcept { return values_.isNull(); }
    bool* data() noexcept { return values_.data(); }
    const bool* data() const noexcept { return values_.data(); }
    bool* begin() noexcept { return data(); }
    const bool* begin() const noexcept { return data(); }
    bool* end() noexcept { return size() ? data() + size() : data(); }
    const bool* end() const noexcept { return size() ? data() + size() : data(); }
    bool& at(std::size_t index) { return values_[index]; }
    const bool& at(std::size_t index) const { return values_[index]; }
    bool& operator[](std::size_t index) { return at(index); }
    const bool& operator[](std::size_t index) const { return at(index); }
    SelectorArray slice(std::size_t first, std::size_t last) const {
        SelectorArray result;
        result.values_ = values_.slice(first, last);
        return result;
    }
    void clear() { values_.resize(0); }
    void push_back(bool value) { values_.append(value); }
    template<class... Args> void emplace_back(Args&&... args) {
        push_back(bool(std::forward<Args>(args)...));
    }
    void appendRange(SelectorArray values) {
        const auto count = values.size();
        if (!count) return;
        if (count > (std::numeric_limits<std::size_t>::max)() - size())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        const auto previous = values_;
        compat::Slice<bool> replacement;
        replacement.resize(size() + count);
        for (std::size_t i = 0; i < size(); ++i) replacement[i] = previous[i];
        for (std::size_t i = 0; i < count; ++i) replacement[size() + i] = values[i];
        values_ = std::move(replacement);
    }
};

// Source void[] is an opaque byte extent and a void pointer, never void values
// or references. An explicit retained view keeps the actual producer allocation.
template<> class SelectorArray<void> {
    compat::Slice<std::byte> bytes_;
public:
    SelectorArray() = default;
    SelectorArray(const SelectorArray&) = default;
    SelectorArray& operator=(const SelectorArray&) = default;
    SelectorArray(SelectorArray&&) noexcept = default;
    SelectorArray& operator=(SelectorArray&&) noexcept = default;
    explicit SelectorArray(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}
    static SelectorArray retainedView(std::shared_ptr<void> owner, void* bytes, std::size_t length) {
        SelectorArray result;
        result.bytes_ = compat::Slice<std::byte>::retainedView(std::move(owner),
            static_cast<std::byte*>(bytes), length);
        return result;
    }
    auto owner() const noexcept { return bytes_.retentionOwner(); }
    std::size_t size() const noexcept { return bytes_.size(); }
    bool empty() const noexcept { return bytes_.empty(); }
    bool isNull() const noexcept { return bytes_.isNull(); }
    void* data() noexcept { return bytes_.data(); }
    const void* data() const noexcept { return bytes_.data(); }
    SelectorArray slice(std::size_t first, std::size_t last) const {
        SelectorArray result;
        result.bytes_ = bytes_.slice(first, last);
        return result;
    }
    void clear() { bytes_.resize(0); }
    void appendRange(SelectorArray values) {
        const auto count = values.size();
        if (!count) return;
        if (count > (std::numeric_limits<std::size_t>::max)() - size())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        const auto previous = bytes_;
        compat::Slice<std::byte> replacement;
        replacement.resize(size() + count);
        for (std::size_t i = 0; i < size(); ++i) replacement[i] = previous[i];
        for (std::size_t i = 0; i < count; ++i) replacement[size() + i] = values.bytes_[i];
        bytes_ = std::move(replacement);
    }
};

// Readonly values need either the existing scalar representation or a real
// declaration-side readonly value binding. Class references use their own carrier.
template<class T> requires (!std::is_volatile_v<T> &&
    (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_void_v<T> ||
        compat::SourceReadonlyValueElement<T>))
class SelectorArray<const T> {
    using StorageElement = std::conditional_t<std::is_void_v<T>, std::byte, T>;
    compat::Slice<StorageElement> values_;
public:
    SelectorArray() = default;
    SelectorArray(const SelectorArray&) = default;
    SelectorArray& operator=(const SelectorArray&) = default;
    SelectorArray(SelectorArray&&) noexcept = default;
    SelectorArray& operator=(SelectorArray&&) noexcept = default;
    SelectorArray(SelectorArray<T> values)
        : SelectorArray(retainedView(values.owner(), values.data(), values.size())) {}
    explicit SelectorArray(std::vector<StorageElement> values) : values_(std::move(values)) {}
    SelectorArray(std::initializer_list<StorageElement> values) : values_(values) {}
    static SelectorArray retainedView(std::shared_ptr<const void> owner, const T* values, std::size_t length) {
        if (length && (owner.use_count() == 0 || !values))
            throw std::invalid_argument("Retained readonly array requires backing");
        const auto* cells = [&] {
            if constexpr (std::is_void_v<T>) return static_cast<const StorageElement*>(values);
            else return values;
        }();
        auto* storage = const_cast<StorageElement*>(cells);
        SelectorArray result;
        // Retain the actual allocation; existing cells are only read. All
        // writes below target a new allocation, never this producer's memory.
        result.values_ = compat::Slice<StorageElement>::retainedView(
            std::shared_ptr<void>(std::move(owner), storage), storage, length);
        return result;
    }
    std::shared_ptr<const void> owner() const noexcept { return values_.retentionOwner(); }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
    bool isNull() const noexcept { return values_.isNull(); }
    const T* data() const noexcept { return values_.data(); }
    const T* begin() const noexcept requires (!std::is_void_v<T>) { return data(); }
    const T* end() const noexcept requires (!std::is_void_v<T>) {
        return size() ? data() + size() : data();
    }
    decltype(auto) at(std::size_t index) const requires (!std::is_void_v<T>) {
        if constexpr (compat::SourceReadonlyValueElement<T>)
            return compat::SourceReadonlyValueBinding<T>::read(values_[index]);
        else return (values_[index]);
    }
    decltype(auto) operator[](std::size_t index) const requires (!std::is_void_v<T>) { return at(index); }
    SelectorArray slice(std::size_t first, std::size_t last) const {
        SelectorArray result;
        result.values_ = values_.slice(first, last);
        return result;
    }
    void clear() { values_.resize(0); }
    void push_back(StorageElement value) requires (!std::is_void_v<T>) {
        const auto previous = values_;
        const auto count = previous.size();
        if (count == (std::numeric_limits<std::size_t>::max)())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        compat::Slice<StorageElement> replacement;
        replacement.resize(count + 1);
        for (std::size_t i = 0; i < count; ++i) replacement[i] = previous[i];
        replacement[count] = value;
        values_ = std::move(replacement);
    }
    template<class... Args> void emplace_back(Args&&... args) requires (!std::is_void_v<T>) {
        push_back(StorageElement(std::forward<Args>(args)...));
    }
    void appendRange(SelectorArray values) {
        const auto previous = values_;
        const auto count = values.size();
        if (!count) return;
        const auto oldLength = previous.size();
        if (count > (std::numeric_limits<std::size_t>::max)() - oldLength)
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        compat::Slice<StorageElement> replacement;
        replacement.resize(oldLength + count);
        for (std::size_t i = 0; i < oldLength; ++i) replacement[i] = previous[i];
        for (std::size_t i = 0; i < count; ++i) replacement[oldLength + i] = values.values_[i];
        values_ = std::move(replacement);
    }
};

struct Token {
    enum class Type : std::int32_t {
        Identifier, Digits, Equals, NotEquals, Greater, GreaterEq, Less, LessEq,
        Assign, LLParen, RLParen, LMParen, RMParen, LSParen, RSParen, Comma,
        Dot, Quote, Space, Colon, Plus, Minus, Multiply, Division, Sharp, Hash,
        Dollar, Question, String, Invalid
    };
    Type type = Type::Identifier;
    nijigenerate::compat::RetainedStringSlice literal;
    Token() = default;
    Token(const Token&) = default;
    Token& operator=(const Token&) = default;
    // Native transport must preserve the source descriptor as a D value copy does.
    Token(Token&& other) noexcept : type(other.type), literal(other.literal) {}
    Token& operator=(Token&& other) noexcept {
        type = other.type;
        literal = other.literal;
        return *this;
    }
    explicit Token(Type type);
    Token(Type type, nijigenerate::compat::RetainedStringSlice literal);
    // Native byte-owning callers explicitly cross into the retained D string API.
    Token(Type type, std::string literal);
    Token(Type type, const char* literal);
    bool match(std::string_view text) const;
    nijigenerate::compat::RetainedStringSlice toString() const;
    // D opEquals is directional: a pattern on the left may have an empty literal.
    bool opEquals(const Token& rhs) const;
    bool equals(Type rhs) const;
    bool equals(std::string_view rhs) const;
};

char32_t ngDecodeSelectorUtf8(std::string_view text, std::size_t& position);

// One actual std.utf carrier across modules; keep the existing native name as
// an alias rather than creating a second class with the same D type name.
using SelectorUtfException = compat::UTFException;

namespace detail {
struct SelectorIntegerHash {
    // The pinned integral hash returns the value converted to size_t.
    std::size_t operator()(int value) const noexcept { return static_cast<std::size_t>(value); }
};
struct SelectorCodepointHash {
    std::size_t operator()(char32_t value) const noexcept { return static_cast<std::size_t>(value); }
};
template<class Value> using SelectorIntegerMap = nicxlive::core::common::ResourceKeyMap<
    int, Value, SelectorIntegerHash, std::equal_to<int>>;
}

class Tokenizer : public std::enable_shared_from_this<Tokenizer> {
public:
    SelectorArray<Token> reservedWord;
    nicxlive::core::common::StringKeyMap<std::shared_ptr<Token>> reservedDict;
    using Transitions = nicxlive::core::common::ResourceKeyMap<char32_t, std::int32_t,
        detail::SelectorCodepointHash, std::equal_to<char32_t>>;
    detail::SelectorIntegerMap<Transitions> reservedWordMap;

    Tokenizer();
    virtual ~Tokenizer() = default;
    // D classes have identity; copying would leave reservedDict pointing at another instance.
    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    virtual void build();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Tokenizer> other);
    virtual int opCmp(std::shared_ptr<Tokenizer> other);
    template<bool useMap = false>
    void tokenize(nijigenerate::compat::RetainedStringSlice text, std::size_t position,
                  SelectorArray<Token>& tokens, std::size_t& nextPosition, bool skipSpace = true) {
        const auto keepAlive = weak_from_this().lock();
        tokenizeImpl(this, useMap, text, position, tokens, nextPosition, skipSpace);
    }
    // Direct Source template calls initialize out arguments before accessing
    // this. A retained nullable receiver avoids a C++ null-member invocation.
    template<bool useMap = false>
    static void tokenizeSource(std::shared_ptr<Tokenizer> receiver,
            nijigenerate::compat::RetainedStringSlice text, std::size_t position,
            SelectorArray<Token>& tokens, std::size_t& nextPosition, bool skipSpace = true) {
        tokenizeImpl(receiver.get(), useMap, text, position, tokens, nextPosition, skipSpace);
    }
    template<bool useMap = false>
    void tokenize(std::string_view text, std::size_t position, SelectorArray<Token>& tokens,
                  std::size_t& nextPosition, bool skipSpace = true) {
        tokenize<useMap>(nijigenerate::compat::RetainedStringSlice(
            nijigenerate::compat::RetainedString(text)), position, tokens, nextPosition, skipSpace);
    }
    // Byte-owning native clients retain their vector transport; D-facing calls use SelectorArray.
    template<bool useMap = false>
    void tokenize(nijigenerate::compat::RetainedStringSlice text, std::size_t position,
                  std::vector<Token>& tokens, std::size_t& nextPosition, bool skipSpace = true) {
        const auto keepAlive = weak_from_this().lock();
        tokenizeImpl(this, useMap, text, position, tokens, nextPosition, skipSpace);
    }
    template<bool useMap = false>
    void tokenize(std::string_view text, std::size_t position, std::vector<Token>& tokens,
                  std::size_t& nextPosition, bool skipSpace = true) {
        tokenize<useMap>(nijigenerate::compat::RetainedStringSlice(
            nijigenerate::compat::RetainedString(text)), position, tokens, nextPosition, skipSpace);
    }
protected:
    virtual void prepareSourceClassInfo();
private:
    template<class Output>
    static void tokenizeImpl(Tokenizer* receiver, bool useMap, nijigenerate::compat::RetainedStringSlice text,
                            std::size_t position, Output& tokens, std::size_t& nextPosition, bool skipSpace);
};

class Scanner : public std::enable_shared_from_this<Scanner> {
public:
    SelectorArray<Token> tokens;
    std::int32_t index = 0;
    explicit Scanner(SelectorArray<Token> tokens);
    explicit Scanner(std::shared_ptr<std::vector<Token>> tokens);
    explicit Scanner(std::vector<Token> tokens);
    virtual ~Scanner() = default;
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    virtual Token scan();
    virtual void rewind(std::int32_t num);
    virtual std::shared_ptr<Scanner> dup();
    virtual bool isEnd();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Scanner> other);
    virtual int opCmp(std::shared_ptr<Scanner> other);
protected:
    virtual void prepareSourceClassInfo();
};

}

namespace nijigenerate::core::translation {
template<> struct FormatSourceTypeName<::nijigenerate::core::selector::Token> {
    static std::string get() { return detail::copyFormatTypeName("Token"); }
};
template<> struct FormatEnumTraits<::nijigenerate::core::selector::Token::Type> {
    using Enum = ::nijigenerate::core::selector::Token::Type;
    inline static constexpr std::string_view name = "Type";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Identifier, "Identifier"},
        FormatEnumMember<Enum>{Enum::Digits, "Digits"},
        FormatEnumMember<Enum>{Enum::Equals, "Equals"},
        FormatEnumMember<Enum>{Enum::NotEquals, "NotEquals"},
        FormatEnumMember<Enum>{Enum::Greater, "Greater"},
        FormatEnumMember<Enum>{Enum::GreaterEq, "GreaterEq"},
        FormatEnumMember<Enum>{Enum::Less, "Less"},
        FormatEnumMember<Enum>{Enum::LessEq, "LessEq"},
        FormatEnumMember<Enum>{Enum::Assign, "Assign"},
        FormatEnumMember<Enum>{Enum::LLParen, "LLParen"},
        FormatEnumMember<Enum>{Enum::RLParen, "RLParen"},
        FormatEnumMember<Enum>{Enum::LMParen, "LMParen"},
        FormatEnumMember<Enum>{Enum::RMParen, "RMParen"},
        FormatEnumMember<Enum>{Enum::LSParen, "LSParen"},
        FormatEnumMember<Enum>{Enum::RSParen, "RSParen"},
        FormatEnumMember<Enum>{Enum::Comma, "Comma"},
        FormatEnumMember<Enum>{Enum::Dot, "Dot"},
        FormatEnumMember<Enum>{Enum::Quote, "Quote"},
        FormatEnumMember<Enum>{Enum::Space, "Space"},
        FormatEnumMember<Enum>{Enum::Colon, "Colon"},
        FormatEnumMember<Enum>{Enum::Plus, "Plus"},
        FormatEnumMember<Enum>{Enum::Minus, "Minus"},
        FormatEnumMember<Enum>{Enum::Multiply, "Multiply"},
        FormatEnumMember<Enum>{Enum::Division, "Division"},
        FormatEnumMember<Enum>{Enum::Sharp, "Sharp"},
        FormatEnumMember<Enum>{Enum::Hash, "Hash"},
        FormatEnumMember<Enum>{Enum::Dollar, "Dollar"},
        FormatEnumMember<Enum>{Enum::Question, "Question"},
        FormatEnumMember<Enum>{Enum::String, "String"},
        FormatEnumMember<Enum>{Enum::Invalid, "Invalid"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<std::shared_ptr<::nijigenerate::core::selector::Token>> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.tokenizer.Token*)";
    inline static constexpr char unqualifiedArgument[] = "(Token*)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.tokenizer.Token*";
    inline static constexpr char unqualifiedElement[] = "Token*";
};

template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Token> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.tokenizer.Token)";
    inline static constexpr char unqualifiedArgument[] = "(Token)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.tokenizer.Token";
    inline static constexpr char unqualifiedElement[] = "Token";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Token::Type> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.tokenizer.Token.Type)";
    inline static constexpr char unqualifiedArgument[] = "(Type)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.tokenizer.Token.Type";
    inline static constexpr char unqualifiedElement[] = "Type";
};
template<> struct SourceTypeArgumentName<::nijigenerate::core::selector::Token> {
    inline static constexpr char argumentName[] = "(nijigenerate.core.selector.tokenizer.Token)";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Tokenizer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.tokenizer.Tokenizer)";
    inline static constexpr char unqualifiedArgument[] = "(Tokenizer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.tokenizer.Tokenizer";
    inline static constexpr char unqualifiedElement[] = "Tokenizer";
};
template<> struct SourceObjectTypeNames<::nijigenerate::core::selector::Tokenizer> {
    inline static constexpr char className[] = "nijigenerate.core.selector.tokenizer.Tokenizer";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.tokenizer.Tokenizer)";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::selector::Scanner> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.tokenizer.Scanner)";
    inline static constexpr char unqualifiedArgument[] = "(Scanner)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.tokenizer.Scanner";
    inline static constexpr char unqualifiedElement[] = "Scanner";
};
template<> struct SourceObjectTypeNames<::nijigenerate::core::selector::Scanner> {
    inline static constexpr char className[] = "nijigenerate.core.selector.tokenizer.Scanner";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.tokenizer.Scanner)";
};
template<class T> struct SourceTypeSpellings<::nijigenerate::core::selector::SelectorArray<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceDynamicArrayTypeSpellings<T>(); }
};
template<class T> requires (!std::is_volatile_v<T> &&
    (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_void_v<T> ||
        ::nijigenerate::compat::SourceReadonlyValueElement<T>))
struct SourceTypeSpellings<::nijigenerate::core::selector::SelectorArray<const T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto qualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
        const auto unqualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
        try {
            auto q = std::string("const(") + std::string(qualified.view()) + ")[]";
            auto u = std::string("const(") + std::string(unqualified.view()) + ")[]";
            return {RetainedString("(" + q + ")"), RetainedString("(" + u + ")"),
                RetainedString(std::move(q)), RetainedString(std::move(u))};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};
}

namespace nijigenerate::core::selector {

// reservedDict stores Source Token*, retained as an alias of its real token
// allocation. Pointer formatting uses that address, never Token.toString().
inline translation::TypedFormatArgument ngFormatArgument(const std::shared_ptr<Token>& value) {
    return translation::FormatPointer{reinterpret_cast<std::uintptr_t>(value.get())};
}

template<class T> requires (!std::is_volatile_v<T>)
translation::TypedFormatArgument ngFormatArgument(SelectorArray<T> values) {
    using namespace translation;
    using Element = std::remove_cv_t<T>;
    auto range = std::allocate_shared<FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
    range->length = values.size();
    // Phobos formats void[] through an actual const ubyte[] view of its bytes.
    using FormattedArray = std::conditional_t<std::is_void_v<Element>,
        SelectorArray<const unsigned char>, SelectorArray<T>>;
    const auto name = nicxlive::core::d_runtime::ngSourceTypeSpelling<FormattedArray>(
        nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
    range->sourceTypeName = translation::detail::copyFormatTypeName(name.view());
    if constexpr (!FormatArrayElement<Element>) {
        range->characterBits = static_cast<unsigned>(sizeof(Element) * CHAR_BIT);
        range->characterView = std::basic_string_view<Element>(values.data(), values.size());
    }
    // Retain the actual descriptor, including readonly bindings and bool cells.
    // Element conversion happens only when the canonical formatter reads it.
    try {
        range->read = [values = std::move(values)](std::size_t index) {
            using translation::ngFormatArgument;
            if constexpr (std::is_void_v<Element>) {
                const auto* bytes = static_cast<const unsigned char*>(values.data());
                return ngFormatArgument(bytes[index]);
            } else return ngFormatArgument(values[index]);
        };
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return range;
}

} // namespace nijigenerate::core::selector

namespace nicxlive::core::serde {

template<> struct SourceJsonPointer<std::shared_ptr<::nijigenerate::core::selector::Token>> {
    using Pointee = ::nijigenerate::core::selector::Token;
};

template<> struct SourceJsonPointer<std::shared_ptr<const ::nijigenerate::core::selector::Token>> {
    using Pointee = const ::nijigenerate::core::selector::Token;
};

template<> struct SourceJsonFields<::nijigenerate::core::selector::Token> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::Token& value, Serializer& serializer) {
        auto state = serializer.structBegin();
        auto type = value.type;
        serializer.putEscapedKey("type");
        serializer.serializeValue(type);
        auto literal = value.literal;
        serializer.putEscapedKey("literal");
        serializer.serializeValue(literal);
        serializer.structEnd(state);
    }
};

template<> struct SourceJsonEnum<::nijigenerate::core::selector::Token::Type> {
    template<class Serializer>
    static void serialize(::nijigenerate::core::selector::Token::Type value, Serializer& serializer) {
        ::nijigenerate::compat::detail::serializePlainSourceEnum(value, serializer);
    }
};

template<> struct SourceJsonEnumKey<::nijigenerate::core::selector::Token::Type> {
    static d_runtime::RetainedString get(::nijigenerate::core::selector::Token::Type value) {
        return ::nijigenerate::compat::detail::enumJsonKey(value);
    }
};

template<JsonArrayElement T>
struct SourceJsonArray<::nijigenerate::core::selector::SelectorArray<T>> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::SelectorArray<T>& value, Serializer& serializer) {
        auto retained = value;
        if (retained.isNull()) { serializer.putValue(nullptr); return; }
        auto state = serializer.listBegin();
        const auto length = retained.size();
        for (std::size_t index = 0; index < length; ++index) {
            auto& element = retained[index];
            serializer.elemBegin();
            serializer.serializeValue(element);
        }
        serializer.listEnd(state);
    }
};

} // namespace nicxlive::core::serde

namespace nijigenerate::core::translation {

template<>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::Tokenizer>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::Tokenizer>> {};

template<>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::Scanner>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::Scanner>> {};

} // namespace nijigenerate::core::translation

#include <nijigenerate/core/typed_format_associative.hpp>

namespace nijigenerate::core::selector {
template<class K, class V, class Hash, class Equal>
::nijigenerate::core::translation::TypedFormatArgument ngFormatArgument(SelectorMap<K, V, Hash, Equal> value) {
    return ::nijigenerate::core::translation::detail::makeAssociativeArgument(std::move(value));
}
}
