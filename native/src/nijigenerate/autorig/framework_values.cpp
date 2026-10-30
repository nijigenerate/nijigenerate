#include "framework.hpp"
#include "json.hpp"
#include "detail/framework_runtime.hpp"
#include "detail/framework_counters.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <cstdlib>
#include <cstring>

namespace nijigenerate::autorig::framework {
namespace detail {
compat::Slice<std::uint8_t> readBytes(std::string_view path) {
    return io::project_reader::ngReadProjectFile(compat::RetainedString(std::string(path)));
}
Json readJson(std::string_view path) {
    const auto bytes = readBytes(path);
    return json::ngParseAutoRigJson(compat::RetainedString(std::string(
        reinterpret_cast<const char*>(bytes.data()), bytes.size())));
}
Json copyJson(Json value) {
    // Phobos std.json object ref getter: Jeremie Pelletier 2008-2009,
    // Boost Software License 1.0, port/licenses/phobos-rig-json.txt.
    if (value.is_object()) {
        const auto& input = value.sourceObjectRef();
        core::settings::SettingsJsonObject result;
        for (const auto& member : input) result[member.name] = copyJson(member.item);
        return result;
    }
    if (value.is_array()) {
        const auto input = value.sourceArrayValue();
        core::settings::SettingsJsonArray result;
        try {
            // Json's actual default constructor is noexcept; recursive/user
            // value copies happen below, outside this pure allocation region.
            result = core::settings::SettingsJsonArray(std::vector<Json>(input.size()));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        for (std::size_t index = 0; index < input.size(); ++index) result[index] = copyJson(input[index]);
        return result;
    }
    return value;
}
}
class NativeBlobPayload final {
    const std::uint8_t* pointer = nullptr;
    std::size_t length_ = 0;
    bool registered = false;
    void prepareSourceClassInfo();
public:
    virtual Text toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<NativeBlobPayload>);
    virtual int opCmp(std::shared_ptr<NativeBlobPayload>);
    NativeBlobPayload(const NativeBlobPayload&) = delete;
    NativeBlobPayload& operator=(const NativeBlobPayload&) = delete;
    explicit NativeBlobPayload(compat::Slice<std::uint8_t> source) {
        prepareSourceClassInfo();
        if (!source.empty()) {
            auto memory = std::malloc(source.size());
            if (!memory) throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            std::memcpy(memory, source.data(), source.size());
            pointer = static_cast<const std::uint8_t*>(memory);
        }
        length_ = source.size();
        detail::nativeBlobBytes.fetch_add(length_);
        detail::nativeBlobCount.fetch_add(1);
        registered = true;
    }
    std::size_t byteLength() const noexcept { return length_; }
    std::span<const std::uint8_t> data() const noexcept { return {pointer, length_}; }
    virtual ~NativeBlobPayload() noexcept {
        std::free(const_cast<std::uint8_t*>(pointer));
        if (registered) {
            detail::nativeBlobBytes.fetch_sub(length_);
            detail::nativeBlobCount.fetch_sub(1);
        }
    }
};
} // namespace nijigenerate::autorig::framework

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::framework::NativeBlobPayload> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.framework.NativeBlobPayload)";
    inline static constexpr char unqualifiedArgument[] = "(NativeBlobPayload)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.framework.NativeBlobPayload";
    inline static constexpr char unqualifiedElement[] = "NativeBlobPayload";
};
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::framework::NativeBlobPayload> {
    inline static constexpr char className[] = "nijigenerate.autorig.framework.NativeBlobPayload";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.framework.NativeBlobPayload)";
};
}

namespace nijigenerate::autorig::framework {
// Define the Object protocol on each actual Source class, with no wrapper base.
#define NG_FRAMEWORK_OBJECT_DEFAULTS(Type, ExceptionSpecification) \
void Type::prepareSourceClassInfo() { \
    detail::registerClass<Type>("nijigenerate.autorig.framework." #Type); \
} \
Text Type::toString() { \
    prepareSourceClassInfo(); \
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)); \
} \
std::size_t Type::toHash() ExceptionSpecification { return detail::objectHash(this); } \
bool Type::opEquals(std::shared_ptr<Type> other) { return detail::objectEquals(this, other); } \
int Type::opCmp(std::shared_ptr<Type>) { \
    prepareSourceClassInfo(); \
    return detail::objectCompare(nicxlive::core::d_runtime::ngObjectClassName(typeid(*this))); \
}
NG_FRAMEWORK_OBJECT_DEFAULTS(NativeBlobPayload, noexcept)
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigProcessor, )
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigTaskContext, )
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigSession, )
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigInputContext, )
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigOutputContext, )
NG_FRAMEWORK_OBJECT_DEFAULTS(AutoRigSessionManager, )
#undef NG_FRAMEWORK_OBJECT_DEFAULTS
AutoRigProcessor::AutoRigProcessor() { prepareSourceClassInfo(); }
std::array<std::uint64_t, 2> ngAutoRigNativeBlobMemoryInfo() {
    return {detail::nativeBlobCount.load(), detail::nativeBlobBytes.load()};
}
compat::Slice<std::uint8_t>& AutoRigValue::bytes() {
    if (nativePayload) {
        mutableBytes = compat::Slice<std::uint8_t>(nativePayload->data()); nativePayload.reset();
    }
    return mutableBytes;
}
std::size_t AutoRigValue::blobByteLength() const { return nativePayload ? nativePayload->byteLength() : mutableBytes.size(); }
AutoRigValue AutoRigValue::fileName(Text value) { AutoRigValue r; r.text = std::move(value); return r; }
AutoRigValue AutoRigValue::path(Text value) { auto r = fileName(std::move(value)); r.kind = AutoRigValueKind::Path; return r; }
AutoRigValue AutoRigValue::jsonValue(Json value) { AutoRigValue r; r.kind = AutoRigValueKind::Json; r.json = std::move(value); return r; }
AutoRigValue AutoRigValue::jsonSnapshot(Json value) {
    AutoRigValue r; r.kind = AutoRigValueKind::Json; r.encodedJson = core::settings::ngSettingsJsonString(value); return r;
}
AutoRigValue AutoRigValue::blob(compat::Slice<std::uint8_t> value) {
    AutoRigValue result; result.kind = AutoRigValueKind::Blob;
    result.nativePayload = std::allocate_shared<NativeBlobPayload>(SourceAllocator<NativeBlobPayload>{}, value);
    return result;
}
compat::Slice<std::uint8_t> AutoRigValue::readBlob() {
    detail::enforce(kind == AutoRigValueKind::Blob, [&] { return Text("AutoRig value is not a blob"); }, 142);
    if (!text.empty()) return detail::readBytes(text);
    if (nativePayload) return compat::Slice<std::uint8_t>(nativePayload->data());
    return mutableBytes.duplicate();
}
compat::ConstSlice<std::uint8_t> AutoRigValue::borrowTaskBlob() {
    detail::enforce(kind == AutoRigValueKind::Blob, [&] { return Text("AutoRig value is not a blob"); }, 149);
    if (!text.empty()) return compat::ConstSlice<std::uint8_t>(detail::readBytes(text).duplicate());
    if (nativePayload) return compat::ConstSlice<std::uint8_t>::retainedView(
        nativePayload, nativePayload->data().data(), nativePayload->byteLength());
    return compat::ConstSlice<std::uint8_t>(mutableBytes.duplicate());
}
compat::ConstSlice<std::uint8_t> AutoRigValue::readImmutableBlob() {
    detail::enforce(kind == AutoRigValueKind::Blob, [&] { return Text("AutoRig value is not a blob"); }, 156);
    if (!text.empty()) return compat::ConstSlice<std::uint8_t>(detail::readBytes(text).duplicate());
    if (nativePayload) return compat::ConstSlice<std::uint8_t>(compat::Slice<std::uint8_t>(nativePayload->data()));
    return compat::ConstSlice<std::uint8_t>(mutableBytes.duplicate());
}
AutoRigValueInfo ngAutoRigValueInfo(AutoRigValue value, std::uint64_t revision) {
    return {value.kind, value.text, value.encodedJson.empty() ? value.blobByteLength() : value.encodedJson.size(), revision};
}
AutoRigValue ngCopyAutoRigValue(AutoRigValue value) {
    switch (value.kind) {
    case AutoRigValueKind::FileName: return AutoRigValue::fileName(value.text);
    case AutoRigValueKind::Path: return AutoRigValue::path(value.text);
    case AutoRigValueKind::Json: return AutoRigValue::jsonValue(!value.text.empty() ? detail::readJson(value.text) :
        !value.encodedJson.empty() ? json::ngParseAutoRigJson(compat::RetainedString(value.encodedJson)) : detail::copyJson(value.json));
    case AutoRigValueKind::Blob: return !value.text.empty() || value.nativePayload ? value : AutoRigValue::blob(value.mutableBytes);
    }
    throw compat::SwitchError(Text::fromStaticLiteral("No appropriate switch clause found"),
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 190);
}
#define NG_PROCESSOR_UI(method, Interface, call, Context) \
bool AutoRigProcessor::method(Text id, std::shared_ptr<Context> context) { \
    auto self = shared_from_this(); \
    auto ui = dynamic_cast<Interface*>(this); if (!ui) return false; \
    ui->call(std::move(id), std::move(context)); return true; \
}
NG_PROCESSOR_UI(renderSessionInputUI, IAutoRigSessionEditor, configureSession, IAutoRigSessionEditContext)
NG_PROCESSOR_UI(renderSessionOutputUI, IAutoRigSessionViewer, viewSession, IAutoRigSessionViewContext)
NG_PROCESSOR_UI(renderInputUI, IAutoRigInputEditor, configureInput, IAutoRigInputContext)
NG_PROCESSOR_UI(renderOutputUI, IAutoRigOutputViewer, viewOutput, IAutoRigOutputContext)
#undef NG_PROCESSOR_UI
}
