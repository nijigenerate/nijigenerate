#pragma once
#include <nodes/node.hpp>
#include <serde_reader.hpp>
#include <serialization_interface.hpp>
#include <type_id.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <new>
#include <source_location>
#include <string>
#include <utility>
namespace nicxlive::core { class Puppet; }
namespace nijigenerate::windows::flipconfig {
class FlipPair : public nicxlive::core::fmt::ISerializable<FlipPair>,
    public std::enable_shared_from_this<FlipPair> {
public:
    std::array<std::shared_ptr<nicxlive::core::nodes::Node>, 2> parts{};
    std::array<std::uint32_t, 2> uuids{};
    compat::RetainedString name;
    FlipPair() = default;
    FlipPair(const FlipPair&) = delete;
    FlipPair& operator=(const FlipPair&) = delete;
    static std::shared_ptr<FlipPair> create(decltype(parts) values, compat::RetainedString label) {
        std::shared_ptr<FlipPair> result;
        try {
            result.reset(new FlipPair(SourceFieldsOnly{}));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        result->constructFlipPairOwned(values, label);
        return result;
    }
    virtual ~FlipPair() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<FlipPair> other);
    virtual bool opEquals(std::shared_ptr<FlipPair> other);
    virtual void flip() {}
    virtual void update();
    template<class S> void serialize(S& serializer) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        auto state = serializer.structBegin();
        serializer.putKey("uuid1");
        serializer.putValue(parts[0] ? compat::ngRequireObject(parts[0])->getUUID() : 0xffffffffu);
        serializer.putKey("uuid2");
        serializer.putValue(parts[1] ? compat::ngRequireObject(parts[1])->getUUID() : 0xffffffffu);
        serializer.structEnd(state);
    }
    virtual nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data);
    virtual void reconstruct(std::shared_ptr<nicxlive::core::Puppet>) {}
    virtual void finalize(std::shared_ptr<nicxlive::core::Puppet> puppet);
protected:
    struct SourceFieldsOnly {};
    explicit FlipPair(SourceFieldsOnly) noexcept {}
    // Invoke at the Source super point after the complete real owner exists.
    void constructFlipPairOwned(decltype(parts) values, compat::RetainedString label) {
        parts = values;
        name = label;
        update();
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::windows::flipconfig::FlipPair> {
    inline static constexpr char className[] = "nijigenerate.windows.flipconfig.FlipPair";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.windows.flipconfig.FlipPair)";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::flipconfig::FlipPair> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.flipconfig.FlipPair)";
    inline static constexpr char unqualifiedArgument[] = "(FlipPair)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.flipconfig.FlipPair";
    inline static constexpr char unqualifiedElement[] = "FlipPair";
};
}

namespace nicxlive::core {
template<> struct UserDefinedAttributes<nijigenerate::windows::flipconfig::FlipPair> {
    static auto get() {
        return std::tuple{TypeId{d_runtime::RetainedString::fromStaticLiteral("FlipPair")}};
    }
};
}
