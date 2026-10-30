#pragma once
#include "inimport_fwd.hpp"
#include "psd.hpp"
#include "kra.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nodes/part.hpp>
#include <serde_source_attributes.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <cstddef>
#include <list>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace nijigenerate::actions::node::detail {
// Existing native object-reference fault implementation in actions/node.cpp.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::io::inimport {
namespace detail {
template<class Operation> decltype(auto) allocateImportBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> compat::RetainedString ngImportDocumentTemplateArgument() {
    if constexpr (std::is_same_v<T, psd::PSD>)
        return compat::RetainedString::fromStaticLiteral("(psd.PSD)");
    else if constexpr (std::is_same_v<T, kra::KRA>)
        return compat::RetainedString::fromStaticLiteral("(kra.KRA)");
    else return nicxlive::core::d_runtime::ngObjectTemplateArgumentName<T>();
}

template<class T> compat::RetainedString ngImportTemplateName(std::string_view declaration,
        bool typeArgument) {
    const auto argument = ngImportDocumentTemplateArgument<T>();
    try {
        std::string text = typeArgument ? "(" : "";
        text += "nijigenerate.io.inimport.";
        text.append(declaration);
        text += '!';
        text.append(argument.view());
        if (typeArgument) text += ')';
        else { text += '.'; text.append(declaration); }
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Class> void ngPrepareImportTemplateClass() {
    static const bool registered = [] {
        using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<Class>;
        const auto name = [] {
            if constexpr (requires { Names::className(); })
                return compat::RetainedString(Names::className());
            else if constexpr (requires { Names::className; })
                return compat::RetainedString::fromStaticLiteral(Names::className);
            else return nicxlive::core::d_runtime::ngObjectClassName(typeid(Class));
        }();
        if constexpr (requires { Names::templateArgumentName(); })
            nicxlive::core::d_runtime::ngRegisterObjectTypeNames<Class>(name,
                compat::RetainedString(Names::templateArgumentName()));
        else if constexpr (requires { Names::templateArgumentName; })
            nicxlive::core::d_runtime::ngRegisterObjectTypeNames<Class>(name,
                compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class), name);
        return true;
    }();
    (void)registered;
}

template<class T>
nicxlive::core::d_runtime::SourceTypeSpellingNames ngImportTemplateTypeSpellings(std::string_view declaration) {
    const auto argument = ngImportDocumentTemplateArgument<T>();
    const auto element = [&](bool qualified) {
        return allocateImportBytes([&] {
            std::string text = qualified ? "nijigenerate.io.inimport." : "";
            text.append(declaration);
            text += '!';
            text.append(argument.view());
            return compat::RetainedString(std::move(text));
        });
    };
    const auto qualified = element(true);
    const auto unqualified = element(false);
    const auto grouped = [&](const compat::RetainedString& value) {
        return allocateImportBytes([&] {
            std::string text = "(";
            text.append(value.view());
            text += ')';
            return compat::RetainedString(std::move(text));
        });
    };
    return {grouped(qualified), grouped(unqualified), qualified, unqualified};
}
} // namespace detail

template<class T> struct ImportTraits;
template<> struct ImportTraits<psd::PSD> : psd::Traits<psd::PSD> {};
template<> struct ImportTraits<kra::KRA> : kra::Traits<kra::KRA> {};
template<class T> struct ImportGraph;
template<class T> struct ImportGraphLink {
    ImportGraphLink() = default;
private:
    std::weak_ptr<ImportGraph<T>> owner_;
    friend struct ImportGraph<T>;
};
template<class T> class LayerRef {
public:
    LayerRef() = default;
    explicit LayerRef(bool embedded) : embedded_(embedded) {}
    LayerRef(const LayerRef& value)
        : pointer_(value.pointer_), strong_(value.owner()), target_(value.target_) {}
    LayerRef& operator=(const LayerRef& value) {
        auto retained = value.owner();
        if (embedded_ && context_ && retained)
            ImportGraph<T>::merge(ImportGraph<T>::ownerOf(context_), retained);
        pointer_ = value.pointer_;
        target_ = value.target_;
        strong_ = embedded_ && context_ ? nullptr : std::move(retained);
        return *this;
    }
    IncImportLayer<T>* get() const { return pointer_; }
    std::shared_ptr<IncImportLayer<T>> operator->() const {
        return std::shared_ptr<IncImportLayer<T>>(owner(), pointer_);
    }
    explicit operator bool() const { return pointer_ != nullptr; }
    std::shared_ptr<ImportGraph<T>> owner() const {
        return strong_ ? ImportGraph<T>::root(strong_) : ImportGraph<T>::ownerOf(target_);
    }
    static LayerRef retain(IncImportLayer<T>* pointer, std::shared_ptr<ImportGraph<T>> owner) {
        LayerRef result;
        result.pointer_ = pointer;
        result.strong_ = ImportGraph<T>::root(std::move(owner));
        result.target_ = ImportGraph<T>::linkOf(result.strong_);
        return result;
    }
private:
    void bindContext(const std::shared_ptr<ImportGraphLink<T>>& context) {
        auto retained = owner();
        if (context && retained) ImportGraph<T>::merge(ImportGraph<T>::ownerOf(context), retained);
        context_ = context;
        if (embedded_ && context_) strong_.reset();
    }
    friend class IncImportLayer<T>;
    template<class> friend class LayerArray;
    bool embedded_ = false;
    IncImportLayer<T>* pointer_ = nullptr;
    std::shared_ptr<ImportGraph<T>> strong_;
    std::shared_ptr<ImportGraphLink<T>> target_, context_;
};
// Internal edges use real component locators; exported references retain the root.
// Merging transfers actual records, preserving their addresses and reference identity.
template<class T> class LayerArray {
    struct Slot {
        std::shared_ptr<ImportGraphLink<T>> context;
        LayerRef<T> value{true};
        Slot() = default;
        Slot(const LayerRef<T>& source, std::shared_ptr<ImportGraphLink<T>> owner) {
            bindContext(std::move(owner));
            value = source;
        }
        // Backing-cell relocation must preserve internal graph-edge ownership.
        Slot(const Slot& source) : Slot(source.value, source.context) {}
        Slot& operator=(const Slot& source) {
            if (!context) bindContext(source.context);
            value = source.value;
            return *this;
        }
        void bindContext(std::shared_ptr<ImportGraphLink<T>> owner) {
            context = std::move(owner);
            LayerArray::bindReference(value, context);
        }
    };
public:
    LayerArray() = default;
    explicit LayerArray(bool embedded) : embedded_(embedded) {}
    LayerArray(const LayerArray& value)
        : values_(value.values_), strong_(value.owner()), backing_(value.backing_) {}
    LayerArray& operator=(const LayerArray& value) {
        auto retained = value.owner();
        if (embedded_ && context_ && retained)
            ImportGraph<T>::merge(ImportGraph<T>::ownerOf(context_), retained);
        values_ = value.values_;
        backing_ = value.backing_;
        strong_ = embedded_ && context_ ? nullptr : std::move(retained);
        return *this;
    }
    std::size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    bool isNull() const { return values_.isNull(); }
    LayerRef<T>& operator[](std::size_t index) { return values_[index].value; }
    LayerRef<T> operator[](std::size_t index) const { return values_[index].value; }
    void append(const LayerRef<T>& value) {
        const auto storage = retainStorage(value.owner());
        values_.append(Slot(value, backing_));
    }
    void resize(std::size_t count) {
        const auto previous = values_.size();
        if (count > previous) {
            const auto storage = retainStorage();
            values_.resize(count);
            for (std::size_t i = previous; i < count; ++i) values_[i].bindContext(backing_);
        } else values_.resize(count);
    }
    std::shared_ptr<ImportGraph<T>> owner() const {
        return strong_ ? ImportGraph<T>::root(strong_) : ImportGraph<T>::ownerOf(backing_);
    }
private:
    static void bindReference(LayerRef<T>& value, const std::shared_ptr<ImportGraphLink<T>>& context) {
        value.bindContext(context);
    }
    void bindContext(const std::shared_ptr<ImportGraphLink<T>>& context) {
        auto retained = owner();
        if (context && retained) ImportGraph<T>::merge(ImportGraph<T>::ownerOf(context), retained);
        context_ = context;
        if (embedded_ && context_) strong_.reset();
    }
    std::shared_ptr<ImportGraph<T>> retainStorage(std::shared_ptr<ImportGraph<T>> incoming = {}) {
        auto retained = owner();
        if (!retained && context_) retained = ImportGraph<T>::ownerOf(context_);
        if (!retained) retained = incoming ? incoming : ImportGraph<T>::create();
        if (incoming) ImportGraph<T>::merge(retained, incoming);
        retained = ImportGraph<T>::root(std::move(retained));
        if (!backing_) backing_ = ImportGraph<T>::linkOf(retained);
        if (!embedded_ || !context_) strong_ = retained;
        return retained;
    }
    friend class IncImportLayer<T>;
    bool embedded_ = false;
    compat::Slice<Slot> values_;
    std::shared_ptr<ImportGraph<T>> strong_;
    std::shared_ptr<ImportGraphLink<T>> backing_, context_;
};
template<class T> class IncImportLayer {
public:
    static void* operator new(std::size_t size) {
        return detail::allocateImportBytes([&] { return ::operator new(size); });
    }
    static void* operator new(std::size_t size, std::align_val_t alignment) {
        return detail::allocateImportBytes([&] { return ::operator new(size, alignment); });
    }
    static void operator delete(void* pointer) noexcept { ::operator delete(pointer); }
    static void operator delete(void* pointer, std::align_val_t alignment) noexcept {
        ::operator delete(pointer, alignment);
    }
    virtual ~IncImportLayer() = default;
    IncImportLayer(const IncImportLayer&) = delete;
    IncImportLayer& operator=(const IncImportLayer&) = delete;
    using Traits = ImportTraits<T>;
    compat::RetainedString name;
    bool hidden = false, isLayerGroup = false;
    nicxlive::core::nodes::BlendMode blendMode = nicxlive::core::nodes::BlendMode::Normal;
    bool clipped = false, passThrough = false, clippingReceiver = false;
    LayerRef<T> clippingBase{true};
    typename Traits::Layer imageLayerRef;
    std::int32_t index = 0;
    LayerRef<T> parent{true};
    LayerArray<T> children{true};
    IncImportLayer(typename Traits::Layer layer, bool isGroup, LayerRef<T> parentValue = {},
                  std::int32_t indexValue = 0) {
        constructSource(layer, isGroup, parentValue, indexValue);
    }
    template<class Arguments> IncImportLayer(std::in_place_t, Arguments&& arguments) {
        std::apply([&](auto&&... values) {
            constructSource(values...);
        }, std::forward<Arguments>(arguments)());
    }
private:
    void constructSource(typename Traits::Layer& layer, bool isGroup, LayerRef<T>& parentValue,
                         std::int32_t indexValue) {
        parent = parentValue;
        imageLayerRef = layer;
        name = imageLayerRef.name;
        isLayerGroup = isGroup;
        index = indexValue;
        if constexpr (requires { Traits::isClippingLayer(layer); })
            clipped = Traits::isClippingLayer(layer);
        if constexpr (requires { Traits::isPassThroughGroup(layer); })
            passThrough = isGroup && Traits::isPassThroughGroup(layer);
        using Source = typename Traits::BlendingMode;
        using Target = nicxlive::core::nodes::BlendMode;
        const auto key = layer.blendModeKey;
#define NG_IMPORT_BLEND(member) if (key.value.view() == Source::member.value.view()) blendMode = Target::member; else
        NG_IMPORT_BLEND(Normal) NG_IMPORT_BLEND(Multiply) NG_IMPORT_BLEND(Screen)
        NG_IMPORT_BLEND(Overlay) NG_IMPORT_BLEND(Darken) NG_IMPORT_BLEND(Lighten)
        NG_IMPORT_BLEND(ColorDodge) NG_IMPORT_BLEND(LinearDodge) NG_IMPORT_BLEND(ColorBurn)
        NG_IMPORT_BLEND(HardLight) NG_IMPORT_BLEND(SoftLight) NG_IMPORT_BLEND(Difference)
        NG_IMPORT_BLEND(Exclusion) NG_IMPORT_BLEND(Subtract) blendMode = Target::Normal;
#undef NG_IMPORT_BLEND
    }
public:
    virtual bool requiresClippingSurface() { return isLayerGroup && (clipped || clippingReceiver); }
    virtual compat::RetainedString getLayerPath() {
        if (parent) {
            const auto parentPath = parent->getLayerPath();
            const auto prefix = detail::allocateImportBytes([&] {
                return std::string(parentPath.view()) + "/";
            });
            return detail::allocateImportBytes([&] {
                return compat::RetainedString(prefix + std::string(name.view()));
            });
        }
        return detail::allocateImportBytes([&] {
            return compat::RetainedString(std::string("/") + std::string(name.view()));
        });
    }
    virtual std::int32_t count() {
        std::uint32_t result = 1;
        const auto source = children;
        for (std::size_t i = 0; i < source.size(); ++i) {
            const auto child = source[i];
            if (!child) actions::node::detail::failActionObjectReference();
            result += static_cast<std::uint32_t>(child->count());
        }
        return std::bit_cast<std::int32_t>(result);
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<IncImportLayer<T>> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual compat::RetainedString toString() {
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual int opCmp(std::shared_ptr<IncImportLayer<T>>) {
        prepareSourceClassInfo();
        const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
        const auto message = detail::allocateImportBytes([&] {
            std::string bytes = "need opCmp for class ";
            bytes.append(name.view());
            return compat::RetainedString(std::move(bytes));
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("object.d"), 195);
    }
protected:
    virtual void prepareSourceClassInfo() {
        detail::ngPrepareImportTemplateClass<IncImportLayer<T>>();
    }
private:
    void bindGraphContext(const std::shared_ptr<ImportGraphLink<T>>& context) {
        clippingBase.bindContext(context);
        parent.bindContext(context);
        children.bindContext(context);
    }
    friend struct ImportGraph<T>;
};
template<class T> struct ImportGraph {
    ImportGraph() = default;
    static std::shared_ptr<ImportGraph> create() {
        auto graph = std::allocate_shared<ImportGraph>(
            nicxlive::core::d_runtime::SourceAllocator<ImportGraph>{});
        graph->link_ = std::allocate_shared<ImportGraphLink<T>>(
            nicxlive::core::d_runtime::SourceAllocator<ImportGraphLink<T>>{});
        graph->link_->owner_ = graph;
        graph->links_.push_back(graph->link_);
        return graph;
    }
    static std::shared_ptr<ImportGraph> root(std::shared_ptr<ImportGraph> graph) {
        while (graph && graph->redirect_) graph = graph->redirect_;
        return graph;
    }
    static std::shared_ptr<ImportGraph> ownerOf(const std::shared_ptr<ImportGraphLink<T>>& link) {
        return link ? root(link->owner_.lock()) : nullptr;
    }
    static std::shared_ptr<ImportGraphLink<T>> linkOf(std::shared_ptr<ImportGraph> graph) {
        graph = root(std::move(graph));
        return graph ? graph->link_ : nullptr;
    }
    static void merge(std::shared_ptr<ImportGraph> destination, std::shared_ptr<ImportGraph> source) {
        destination = root(std::move(destination));
        source = root(std::move(source));
        if (!destination || !source || destination == source) return;
        // Transfer the actual owning nodes without allocating during a reference assignment.
        destination->layers_.splice(destination->layers_.end(), source->layers_);
        for (auto& link : source->links_) link->owner_ = destination;
        destination->links_.splice(destination->links_.end(), source->links_);
        // Only emptied former roots retain a new root; the new root never owns them.
        source->redirect_ = destination;
    }
    LayerRef<T> addOwned(std::unique_ptr<IncImportLayer<T>> layer) {
        auto graph = ownerOf(link_);
        layer->bindGraphContext(graph->link_);
        graph = root(std::move(graph));
        const auto pointer = layer.get();
        graph->layers_.push_back(std::move(layer));
        return LayerRef<T>::retain(pointer, graph);
    }
private:
    std::list<std::unique_ptr<IncImportLayer<T>>,
        nicxlive::core::d_runtime::SourceAllocator<std::unique_ptr<IncImportLayer<T>>>> layers_;
    std::list<std::shared_ptr<ImportGraphLink<T>>,
        nicxlive::core::d_runtime::SourceAllocator<std::shared_ptr<ImportGraphLink<T>>>> links_;
    std::shared_ptr<ImportGraphLink<T>> link_;
    std::shared_ptr<ImportGraph> redirect_;
};
template<class T> LayerArray<T> incBuildLayerLayout(T document) {
    using Traits = ImportTraits<T>;
    std::shared_ptr<ImportGraph<T>> graph;
    LayerArray<T> result, stack;
    auto ordered = Traits::layers(document);
    constexpr bool topToBottom = [] {
        if constexpr (requires { Traits::layersTopToBottom; }) return Traits::layersTopToBottom;
        else return false;
    }();
    std::uint32_t index = 0;
    for (auto layer : ordered) {
        --index;
        if (Traits::isGroupEnd(layer)) {
            if (stack.size() == 1) { result.append(stack[0]); stack.resize(0); continue; }
            if (stack.size() > 1) {
                stack[stack.size() - 2]->children.append(stack[stack.size() - 1]);
                stack.resize(stack.size() - 1); continue;
            }
            throw compat::ThrowableException(
                compat::RetainedString::fromStaticLiteral("Unexpected closing layer group"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inimport.d"), 131);
        }
        auto object = std::unique_ptr<IncImportLayer<T>>(new IncImportLayer<T>(std::in_place, [&] {
            auto argumentLayer = layer;
            const auto isGroup = Traits::isGroupStart(layer);
            auto parent = stack.empty() ? LayerRef<T>{} : stack[stack.size() - 1];
            const auto sourceIndex = std::bit_cast<std::int32_t>(index);
            return std::tuple{std::move(argumentLayer), isGroup, std::move(parent), sourceIndex};
        }));
        if (!graph) graph = ImportGraph<T>::create();
        auto current = graph->addOwned(std::move(object));
        if (current->isLayerGroup) stack.append(current);
        else if (!stack.empty()) stack[stack.size() - 1]->children.append(current);
        else result.append(current);
    }
    const auto resolve = [&](auto& self, LayerArray<T> siblings) -> void {
        LayerRef<T> base;
        const auto siblingCount = siblings.size();
        for (std::size_t offset = 0; offset < siblingCount; ++offset) {
            auto layer = siblings[topToBottom ? siblings.size() - 1 - offset : offset];
            if (!layer) actions::node::detail::failActionObjectReference();
            if (layer->clipped) {
                if (!base) {
                    const auto path = layer->getLayerPath();
                    const auto message = detail::allocateImportBytes([&] {
                        std::string bytes = "PSD clipping layer has no base: ";
                        bytes.append(path.view());
                        // Source enforce duplicates the completed lazy message.
                        return compat::RetainedString(std::string_view(bytes));
                    });
                    throw compat::ThrowableException(message,
                        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inimport.d"), 157);
                }
                layer->clippingBase = base; base->clippingReceiver = true;
            } else base = layer;
            self(self, layer->children);
        }
    };
    resolve(resolve, result);
    return result;
}
}

namespace nicxlive::core {
template<class T>
struct MemberDefinedAttributes<::nijigenerate::io::inimport::IncImportLayer<T>> {
    using Layer = ::nijigenerate::io::inimport::IncImportLayer<T>;
    template<auto Member> static auto get() {
        if constexpr (ngSameSourceDeclaration<Member, &Layer::clippingBase> ||
                ngSameSourceDeclaration<Member, &Layer::imageLayerRef> ||
                ngSameSourceDeclaration<Member, &Layer::index> ||
                ngSameSourceDeclaration<Member, &Layer::parent>) {
            return std::tuple{SourceTypeAttribute<serde::source_attributes::SerdeIgnore>{}};
        } else return std::tuple{};
    }
};
} // namespace nicxlive::core

namespace nicxlive::core::d_runtime {
template<class T>
struct SourceTypeSpellings<::nijigenerate::io::inimport::IncImportLayer<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        static_assert(requires { sizeof(::nijigenerate::io::inimport::IncImportLayer<T>); },
            "Source type spelling requires the actual import-layer declaration");
        return ::nijigenerate::io::inimport::detail::ngImportTemplateTypeSpellings<T>("IncImportLayer");
    }
};
template<class T>
struct SourceObjectTypeNames<::nijigenerate::io::inimport::IncImportLayer<T>> {
    static RetainedString className() {
        return ::nijigenerate::io::inimport::detail::ngImportTemplateName<T>("IncImportLayer", false);
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::io::inimport::detail::ngImportTemplateName<T>("IncImportLayer", true);
    }
};

// These are carriers of the actual Source class reference and its array, not
// additional Source class declarations or separate Object/ClassInfo identities.
template<class T>
struct SourceTypeSpellings<::nijigenerate::io::inimport::LayerRef<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        using Layer = ::nijigenerate::io::inimport::IncImportLayer<T>;
        return SourceTypeSpellings<std::shared_ptr<Layer>>::get();
    }
};

template<class T>
struct SourceTypeSpellings<::nijigenerate::io::inimport::LayerArray<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return ngSourceDynamicArrayTypeSpellings<::nijigenerate::io::inimport::LayerRef<T>>();
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {
template<class T>
struct FormatObjectTraits<::nijigenerate::io::inimport::LayerRef<T>>
    : detail::SourceNoArgObjectFormat<::nijigenerate::io::inimport::LayerRef<T>> {};

template<class T>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::io::inimport::IncImportLayer<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::io::inimport::IncImportLayer<T>>> {};
} // namespace nijigenerate::core::translation

namespace nijigenerate::io::inimport {

// ADL keeps the formatter attached to the actual Source array carrier. Retain
// its descriptor so later reads observe the same cells and keep the graph alive.
template<class T>
core::translation::TypedFormatArgument ngFormatArgument(LayerArray<T> values) {
    using namespace core::translation;
    auto range = std::allocate_shared<FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
    range->length = values.size();
    const auto name = nicxlive::core::d_runtime::ngSourceTypeSpelling<LayerArray<T>>(
        nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
    range->sourceTypeName = detail::allocateImportBytes([&] { return std::string(name.view()); });
    detail::allocateImportBytes([&] {
        range->read = [values = std::move(values)](std::size_t index) {
            return core::translation::ngFormatArgument(values[index]);
        };
    });
    return range;
}

} // namespace nijigenerate::io::inimport
