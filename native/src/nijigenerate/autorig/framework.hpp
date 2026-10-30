#pragma once
#include "framework_schema.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <common/resource_key_map.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <concepts>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>
#include <new>
#include <stdexcept>

namespace nijigenerate::autorig::framework {
using Json = core::settings::SettingsJsonValue;
using Text = compat::RetainedString;
using nicxlive::core::d_runtime::SourceAllocator;
// Reuse the actual UUID producer for original modules importing std.uuid.
Text ngAutoRigRandomUuid();
// C++ spelling of this module's D immutable-string concatenations.
inline Text operator+(const Text& left, const Text& right) {
    if (left.empty() && right.empty()) return Text{};
    try {
        std::string bytes(left.view());
        bytes.append(right.view());
        return Text(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct TextHash {
    using is_transparent = void;
    std::size_t operator()(const Text& value) const noexcept { return (*this)(value.view()); }
    std::size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view(value)); }
    std::size_t operator()(const char* value) const noexcept {
        return (*this)(value ? std::string_view(value) : std::string_view{});
    }
    std::size_t operator()(std::string_view value) const noexcept { return compat::ngPhobosStringHash(value); }
};
struct TextEqual {
    using is_transparent = void;
    bool operator()(const Text& a, const Text& b) const noexcept { return a.view() == b.view(); }
    bool operator()(const Text& a, std::string_view b) const noexcept { return a.view() == b; }
    bool operator()(std::string_view a, const Text& b) const noexcept { return a == b.view(); }
    bool operator()(const Text& a, const std::string& b) const noexcept { return a.view() == b; }
    bool operator()(const std::string& a, const Text& b) const noexcept { return a == b.view(); }
    bool operator()(const Text& a, const char* b) const noexcept {
        return a.view() == (b ? std::string_view(b) : std::string_view{});
    }
    bool operator()(const char* a, const Text& b) const noexcept {
        return (a ? std::string_view(a) : std::string_view{}) == b.view();
    }
};
// Keep the canonical AA's actual cells and bucket enumeration. Native callers
// need references to those same cells, including unique-owned context entries.
template<class T> class Table {
    using Storage = nicxlive::core::common::ResourceKeyMap<Text, T, TextHash, TextEqual>;
    Storage entries_;
    template<class Q> static Text lookupKey(const Q& key) {
        if constexpr (std::same_as<std::remove_cvref_t<Q>, Text>) return key;
        else if constexpr (requires { key.retainedString(); }) return key.retainedString();
        else if constexpr (std::same_as<std::remove_cvref_t<Q>, const char*> ||
            std::same_as<std::remove_cvref_t<Q>, char*>)
            return Text::fromBorrowedView(key ? std::string_view(key) : std::string_view{});
        else return Text::fromBorrowedView(std::string_view(key));
    }
public:
    Table() = default;
    Table(const Table&) = default;
    Table& operator=(const Table&) = default;
    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;
    bool empty() const noexcept { return entries_.empty(); }
    bool isNull() const noexcept { return entries_.isNull(); }
    const void* identity() const noexcept { return entries_.identity(); }
    std::size_t size() const noexcept { return entries_.size(); }
    compat::Slice<Text> keys() const {
        try {
            auto copied = entries_.keys();
            return compat::Slice<Text>::retainedView(copied.retentionOwner(), copied.data(), copied.size());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    void reset() noexcept { entries_ = Storage{}; }
    void clear() { entries_.clear(); }
    std::size_t erase(const Text& key) { return entries_.erase(key); }
    auto begin() { return entries_.begin(); }
    auto end() { return entries_.end(); }
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }
    template<class Q> auto find(const Q& key) { return entries_.find(lookupKey(key)); }
    template<class Q> auto find(const Q& key) const { return entries_.find(lookupKey(key)); }
    template<class Q> bool contains(const Q& key) const { return find(key) != end(); }
    T& operator[](const Text& key) { return static_cast<T&>(entries_[key]); }
    template<class Q> T& at(const Q& key) {
        const auto found = find(key);
        if (found == end()) throw compat::ngStaticError<compat::RangeError>(std::source_location::current());
        return found->second;
    }
    template<class Q> const T& at(const Q& key) const {
        const auto found = find(key);
        if (found == end()) throw compat::ngStaticError<compat::RangeError>(std::source_location::current());
        return found->second;
    }
    Table duplicate() const requires std::copy_constructible<T> {
        Table result;
        // object.d:3079-3113 inserts each copied value into a fresh AA in the
        // source table's bucket order; an empty duplicate remains null.
        for (const auto& [key, value] : entries_) result[key] = value;
        return result;
    }
    template<class... Args> auto try_emplace(const Text& key, Args&&... args) {
        auto found = entries_.find(key);
        if (found != entries_.end()) return std::pair{found, false};
        (*this)[key] = T(std::forward<Args>(args)...);
        return std::pair{entries_.find(key), true};
    }
};
using Action = compat::SharedDelegate<void()>;
using AutoRigTaskActionBoundary = compat::SharedDelegate<void(Text, Action)>;
using AutoRigEditorDispatcher = compat::SharedDelegate<void(Action)>;
class NativeBlobPayload;
class AutoRigSession;
class AutoRigTaskContext;
struct AutoRigValueInfo;

struct AutoRigValue {
    AutoRigValueKind kind{};
    Text text;
    Json json;
private:
    compat::Slice<std::uint8_t> mutableBytes;
    std::shared_ptr<const NativeBlobPayload> nativePayload;
    Text encodedJson;
    static AutoRigValue jsonSnapshot(Json value);
    std::size_t blobByteLength() const;
    friend class AutoRigTaskContext;
    friend AutoRigValue ngCopyAutoRigValue(AutoRigValue);
    friend struct AutoRigValueInfo;
    friend AutoRigValueInfo ngAutoRigValueInfo(AutoRigValue, std::uint64_t);
public:
    compat::Slice<std::uint8_t>& bytes();
    static AutoRigValue fileName(Text value);
    static AutoRigValue path(Text value);
    static AutoRigValue jsonValue(Json value);
    static AutoRigValue blob(compat::Slice<std::uint8_t> value);
    compat::Slice<std::uint8_t> readBlob();
    compat::ConstSlice<std::uint8_t> borrowTaskBlob();
    compat::ConstSlice<std::uint8_t> readImmutableBlob();
};
struct AutoRigValueInfo {
    AutoRigValueKind kind{};
    Text text;
    std::size_t byteLength{};
    std::uint64_t revision{};
};
AutoRigValueInfo ngAutoRigValueInfo(AutoRigValue value, std::uint64_t revision);
AutoRigValue ngCopyAutoRigValue(AutoRigValue value);
std::array<std::uint64_t, 2> ngAutoRigNativeBlobMemoryInfo();
struct AutoRigWorkflowSpec {
    Text id, label, description;
    compat::Slice<AutoRigPortSpec> inputs, outputs;
    compat::Slice<AutoRigWorkflowStep> steps;
    compat::Slice<AutoRigWorkflowInputBinding> inputBindings;
    compat::Slice<AutoRigWorkflowConnection> connections;
    compat::Slice<AutoRigWorkflowOutputBinding> outputBindings;
    Table<AutoRigValue> inputDefaults;
    bool preferred{};
};
struct AutoRigArtifact {
    Text taskId, portId;
    std::uint32_t attempt{};
    AutoRigValueKind kind{};
    Text storagePath, mediaType, viewHint;
    bool committed{}, preview{};
private:
    AutoRigValue value_;
    void value(AutoRigValue item) { value_ = std::move(item); }
    friend class AutoRigTaskContext;
public:
    AutoRigValue value() const { return ngCopyAutoRigValue(value_); }
    std::size_t byteLength() const { return ngAutoRigValueInfo(value_, 0).byteLength; }
};
struct AutoRigTaskSnapshot {
    Text taskId;
    std::uint32_t attempt{};
    AutoRigTaskState state{};
    Text message;
    compat::Slice<AutoRigArtifact> artifacts;
};
struct IAutoRigInputContext {
    virtual ~IAutoRigInputContext() = default;
    virtual Text taskId() = 0;
    virtual compat::Slice<AutoRigPortSpec> ports() = 0;
    virtual bool isConnected(Text) = 0;
    virtual bool canEdit(Text) = 0;
    virtual bool hasValue(Text) = 0;
    virtual AutoRigValue value(Text) = 0;
    virtual void setValue(Text, AutoRigValue) = 0;
};
struct IAutoRigOutputContext {
    virtual ~IAutoRigOutputContext() = default;
    virtual Text taskId() = 0;
    virtual compat::Slice<AutoRigPortSpec> ports() = 0;
    virtual AutoRigTaskSnapshot snapshot() = 0;
    virtual bool hasValue(Text) = 0;
    virtual AutoRigValue value(Text) = 0;
    virtual AutoRigArtifact artifact(Text) = 0;
};
struct IAutoRigInputEditor {
    virtual ~IAutoRigInputEditor() = default;
    virtual void configureInput(Text, std::shared_ptr<IAutoRigInputContext>) = 0;
};
struct IAutoRigOutputViewer {
    virtual ~IAutoRigOutputViewer() = default;
    virtual void viewOutput(Text, std::shared_ptr<IAutoRigOutputContext>) = 0;
};
struct IAutoRigSessionViewContext {
    virtual ~IAutoRigSessionViewContext() = default;
    virtual Text runId() = 0;
    virtual Text workflowId() = 0;
    virtual compat::Slice<AutoRigPortSpec> inputPorts() = 0;
    virtual bool hasInput(Text) = 0;
    virtual AutoRigValue input(Text) = 0;
    virtual compat::Slice<Text> contextNames() = 0;
    virtual AutoRigValue contextValue(Text) = 0;
    virtual compat::Slice<AutoRigPortSpec> outputPorts() = 0;
    virtual bool hasOutput(Text) = 0;
    virtual AutoRigValue output(Text) = 0;
    virtual AutoRigArtifact outputArtifact(Text) = 0;
};
struct IAutoRigSessionEditContext : IAutoRigSessionViewContext {
    virtual bool canEdit() = 0;
    virtual void setInput(Text, AutoRigValue) = 0;
    virtual void setContextValue(Text, AutoRigValue) = 0;
};
struct IAutoRigSessionEditor {
    virtual ~IAutoRigSessionEditor() = default;
    virtual void configureSession(Text, std::shared_ptr<IAutoRigSessionEditContext>) = 0;
};
struct IAutoRigSessionViewer {
    virtual ~IAutoRigSessionViewer() = default;
    virtual void viewSession(Text, std::shared_ptr<IAutoRigSessionViewContext>) = 0;
};
class AutoRigProcessor : public std::enable_shared_from_this<AutoRigProcessor> {
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigProcessor();
    AutoRigProcessor(const AutoRigProcessor&) = delete;
    AutoRigProcessor& operator=(const AutoRigProcessor&) = delete;
    virtual ~AutoRigProcessor() = default;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigProcessor>);
    virtual int opCmp(std::shared_ptr<AutoRigProcessor>);
    virtual Text procId() = 0;
    virtual Text displayName() = 0;
    virtual compat::Slice<AutoRigTaskSpec> tasks() = 0;
    virtual compat::Slice<AutoRigWorkflowSpec> workflows() { return {}; }
    virtual void executeTask(Text, std::shared_ptr<AutoRigTaskContext>) = 0;
    virtual bool renderSessionInputUI(Text, std::shared_ptr<IAutoRigSessionEditContext>);
    virtual bool renderSessionOutputUI(Text, std::shared_ptr<IAutoRigSessionViewContext>);
    virtual bool renderInputUI(Text, std::shared_ptr<IAutoRigInputContext>);
    virtual bool renderOutputUI(Text, std::shared_ptr<IAutoRigOutputContext>);
};
struct AutoRigWorkspace {
    virtual ~AutoRigWorkspace() = default;
    virtual Json memoryInfo() = 0;
    virtual void dispose() = 0;
};
class AutoRigTaskContext : public std::enable_shared_from_this<AutoRigTaskContext> {
    AutoRigTaskSpec spec;
    Text attemptDirectory;
    Table<AutoRigValue> inputs, outputs;
    Table<AutoRigArtifact> artifacts;
    Table<Text> reservedPreviews;
    std::uint32_t previewVersion{};
    compat::SharedDelegate<bool()> canceled;
    compat::SharedDelegate<void(AutoRigArtifact)> onPreview;
    AutoRigEditorDispatcher editorDispatcher;
    Text reportedFailure, sessionId_;
    Table<AutoRigValue> sessionValues;
    compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>(Text,
        compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>()>)> workspaceProvider;
    AutoRigPortSpec requireOutput(Text, AutoRigValueKind);
    void publish(AutoRigPortSpec, AutoRigValue, Text);
    void previewValue(Text, AutoRigValue, Text);
    friend class AutoRigSession;
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigTaskContext(AutoRigTaskSpec, Text, Table<AutoRigValue>, compat::SharedDelegate<bool()>,
        compat::SharedDelegate<void(AutoRigArtifact)>, AutoRigEditorDispatcher);
    virtual ~AutoRigTaskContext() = default;
    AutoRigTaskContext(const AutoRigTaskContext&) = delete;
    AutoRigTaskContext& operator=(const AutoRigTaskContext&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigTaskContext>);
    virtual int opCmp(std::shared_ptr<AutoRigTaskContext>);
    virtual Text sessionId() { return sessionId_; }
    virtual std::shared_ptr<AutoRigWorkspace> workspace(Text,
        compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>()>);
    virtual AutoRigValue contextValue(Text);
    virtual compat::Slice<Text> contextNames();
    virtual bool hasInput(Text);
    virtual AutoRigValue input(Text);
    virtual bool isCanceled();
    virtual void reportFailure(Text);
    virtual void runOnMainThread(Action);
    virtual Text outputPath(Text, Text extension = "");
    virtual void publishPath(Text, Text);
    virtual void publishFileName(Text, Text);
    virtual void publishJson(Text, Json);
    virtual void publishBlob(Text, compat::Slice<std::uint8_t>);
    virtual void previewJson(Text, Json, Text mediaType = "application/json");
    virtual void previewBlob(Text, compat::Slice<std::uint8_t>, Text mediaType = "application/octet-stream");
    virtual Text previewPath(Text, Text);
    virtual void publishPreview(Text, Text, Text);
};
class AutoRigInputContext;
class AutoRigOutputContext;
class AutoRigSession : public std::enable_shared_from_this<AutoRigSession> {
    std::shared_ptr<AutoRigProcessor> processor;
    Text id_, directory_;
    Table<AutoRigTaskSpec> specs;
    Table<Table<AutoRigValue>> suppliedInputs, committedOutputs;
    Table<AutoRigTaskSnapshot> snapshots;
    Table<std::shared_ptr<AutoRigInputContext>> inputContexts;
    Table<std::shared_ptr<AutoRigOutputContext>> outputContexts;
    AutoRigTaskActionBoundary actionBoundary;
    AutoRigEditorDispatcher editorDispatcher;
    std::atomic<bool> cancelRequested{false};
    bool busy{};
    Table<AutoRigValue> sessionValues;
    Table<std::uint64_t> inputRevisions;
    std::uint64_t contextRevision_{};
    Table<std::shared_ptr<AutoRigWorkspace>> workspaces;
    std::recursive_mutex mutex_;
    void validateDependencies(Text, Table<std::uint8_t>&);
    void invalidateDependents(Text);
    void runTask(Text, Table<bool>&, bool);
    void finishExecution(compat::Slice<Text>, Table<std::uint32_t>&);
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigSession(std::shared_ptr<AutoRigProcessor>, Text, Text,
        AutoRigTaskActionBoundary, AutoRigEditorDispatcher);
    virtual ~AutoRigSession();
    AutoRigSession(const AutoRigSession&) = delete;
    AutoRigSession& operator=(const AutoRigSession&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigSession>);
    virtual int opCmp(std::shared_ptr<AutoRigSession>);
    virtual void restoreCommittedOutputs();
    virtual Text id() { return id_; }
    virtual Text directory() { return directory_; }
    virtual Text processorId();
    virtual bool isBusy();
    virtual Json memoryInfo();
    virtual void dispose();
    virtual bool isCanceled() { return cancelRequested.load(); }
    virtual void setContextValue(Text, AutoRigValue);
    virtual AutoRigValue contextValue(Text);
    virtual compat::Slice<Text> contextNames();
    virtual AutoRigValueInfo contextInfo(Text);
    virtual std::uint64_t inputRevision(Text);
    virtual void invalidateTask(Text);
    virtual void setInput(Text, Text, AutoRigValue);
    virtual void execute(Text, bool force = false);
    virtual void executeGroup(compat::Slice<Text>, bool force = false);
    virtual void cancel();
    virtual AutoRigTaskSnapshot task(Text, bool includeArtifacts = true);
    virtual AutoRigTaskSpec taskSpec(Text);
    virtual bool hasSuppliedInput(Text, Text);
    virtual AutoRigValue suppliedInput(Text, Text);
    virtual bool hasOutput(Text, Text);
    virtual std::shared_ptr<IAutoRigInputContext> inputContext(Text);
    virtual std::shared_ptr<IAutoRigOutputContext> outputContext(Text);
    virtual bool renderInputUI(Text);
    virtual bool renderOutputUI(Text);
    virtual AutoRigValue output(Text, Text);
    virtual AutoRigArtifact outputArtifact(Text, Text);
};
class AutoRigInputContext : public IAutoRigInputContext {
    AutoRigSession* session_;
    Text taskId_;
    bool findConnection(Text, AutoRigConnection&);
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigInputContext(AutoRigSession* session, Text id);
    AutoRigInputContext(const AutoRigInputContext&) = delete;
    AutoRigInputContext& operator=(const AutoRigInputContext&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigInputContext>);
    virtual int opCmp(std::shared_ptr<AutoRigInputContext>);
    Text taskId() override { return taskId_; }
    compat::Slice<AutoRigPortSpec> ports() override;
    bool isConnected(Text) override;
    bool canEdit(Text) override;
    bool hasValue(Text) override;
    AutoRigValue value(Text) override;
    void setValue(Text, AutoRigValue) override;
};
class AutoRigOutputContext : public IAutoRigOutputContext {
    AutoRigSession* session_;
    Text taskId_;
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigOutputContext(AutoRigSession* session, Text id);
    AutoRigOutputContext(const AutoRigOutputContext&) = delete;
    AutoRigOutputContext& operator=(const AutoRigOutputContext&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigOutputContext>);
    virtual int opCmp(std::shared_ptr<AutoRigOutputContext>);
    Text taskId() override { return taskId_; }
    compat::Slice<AutoRigPortSpec> ports() override;
    AutoRigTaskSnapshot snapshot() override;
    bool hasValue(Text) override;
    AutoRigValue value(Text) override;
    AutoRigArtifact artifact(Text) override;
};
class AutoRigSessionManager : public std::enable_shared_from_this<AutoRigSessionManager> {
    Text root;
    Table<std::shared_ptr<AutoRigProcessor>> processors;
    Table<Text> processorAliases;
    Table<std::shared_ptr<AutoRigSession>> sessions, closedSessions;
    AutoRigTaskActionBoundary actionBoundary;
    AutoRigEditorDispatcher editorDispatcher;
protected:
    virtual void prepareSourceClassInfo();
public:
    explicit AutoRigSessionManager(Text);
    virtual ~AutoRigSessionManager() = default;
    AutoRigSessionManager(const AutoRigSessionManager&) = delete;
    AutoRigSessionManager& operator=(const AutoRigSessionManager&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigSessionManager>);
    virtual int opCmp(std::shared_ptr<AutoRigSessionManager>);
    virtual void registerProcessor(std::shared_ptr<AutoRigProcessor>);
    virtual void registerProcessorAlias(Text, Text);
    virtual void setTaskActionBoundary(AutoRigTaskActionBoundary value) { actionBoundary = std::move(value); }
    virtual void setEditorDispatcher(AutoRigEditorDispatcher value) { editorDispatcher = std::move(value); }
    virtual compat::Slice<std::shared_ptr<AutoRigProcessor>> listProcessors();
    virtual std::shared_ptr<AutoRigSession> create(Text);
    virtual std::shared_ptr<AutoRigSession> createAt(Text, Text);
    virtual std::shared_ptr<AutoRigSession> createWithProcessor(std::shared_ptr<AutoRigProcessor>, Text);
    virtual std::shared_ptr<AutoRigSession> reopenWithProcessor(std::shared_ptr<AutoRigProcessor>, Text);
    virtual std::shared_ptr<AutoRigProcessor> processor(Text);
    virtual Text rootDirectory() { return root; }
    virtual std::shared_ptr<AutoRigSession> get(Text);
    virtual void close(Text);
    virtual void remove(Text);
    virtual void disposeAll();
};
}

namespace nicxlive::core::d_runtime {
#define NG_FRAMEWORK_TYPE_ORIGINS(Type, Reference) \
template<> struct SourceTypeSpellings<::nijigenerate::autorig::framework::Type> { \
    inline static constexpr bool isNativeObject = Reference; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.framework." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.framework." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigValue, false)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigValueInfo, false)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigWorkflowSpec, false)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigArtifact, false)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigTaskSnapshot, false)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigInputContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigOutputContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigInputEditor, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigOutputViewer, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigSessionViewContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigSessionEditContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigSessionEditor, true)
NG_FRAMEWORK_TYPE_ORIGINS(IAutoRigSessionViewer, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigWorkspace, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigProcessor, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigTaskContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigSession, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigInputContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigOutputContext, true)
NG_FRAMEWORK_TYPE_ORIGINS(AutoRigSessionManager, true)
#undef NG_FRAMEWORK_TYPE_ORIGINS

#define NG_FRAMEWORK_OBJECT_NAMES(Type) \
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::framework::Type> { \
    inline static constexpr char className[] = "nijigenerate.autorig.framework." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.framework." #Type ")"; \
};
NG_FRAMEWORK_OBJECT_NAMES(AutoRigProcessor)
NG_FRAMEWORK_OBJECT_NAMES(AutoRigTaskContext)
NG_FRAMEWORK_OBJECT_NAMES(AutoRigSession)
NG_FRAMEWORK_OBJECT_NAMES(AutoRigInputContext)
NG_FRAMEWORK_OBJECT_NAMES(AutoRigOutputContext)
NG_FRAMEWORK_OBJECT_NAMES(AutoRigSessionManager)
#undef NG_FRAMEWORK_OBJECT_NAMES
}
