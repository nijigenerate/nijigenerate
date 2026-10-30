#pragma once

#include "common.hpp"
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::panels::inspector::detail {

template<class Build> compat::RetainedString inspectorNameBytes(Build&& build) {
    try {
        return compat::RetainedString(std::forward<Build>(build)());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString inspectorSingleDeclaration(std::string_view declaration,
    std::string_view arguments, bool qualified, bool classLeaf) {
    return inspectorNameBytes([&] {
        std::string name = qualified ? "nijigenerate.panels.inspector.common." : "";
        name.append(declaration);
        name += '!';
        name.append(arguments);
        if (classLeaf) { name += '.'; name.append(declaration); }
        return name;
    });
}

inline compat::RetainedString inspectorBaseDeclaration(ModelEditSubMode mode, std::string_view element,
    bool qualified, bool classLeaf) {
    return inspectorNameBytes([&] {
        std::string name = qualified ? "nijigenerate.panels.inspector.common." : "";
        name += "BaseInspector!(ModelEditSubMode.";
        name += mode == ModelEditSubMode::Layout ? "Layout, " : "Deform, ";
        name.append(element);
        name += ')';
        if (classLeaf) name += ".BaseInspector";
        return name;
    });
}

inline compat::RetainedString inspectorClassArgument(compat::RetainedString element) {
    return inspectorNameBytes([&] { return "(" + std::string(element.view()) + ")"; });
}

template<class T> compat::RetainedString inspectorDeclaredClassName() {
    using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<T>;
    if constexpr (requires { Names::className(); }) return Names::className();
    else if constexpr (requires { Names::className; })
        return compat::RetainedString::fromStaticLiteral(Names::className);
    else return nicxlive::core::d_runtime::ngObjectClassName(typeid(T));
}

template<class T> void prepareInspectorClassRecord() {
    static const bool registered = [] {
        const auto name = inspectorDeclaredClassName<T>();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T), name);
        return true;
    }();
    (void)registered;
}

// A nested declaration's type printer uses its lexical symbol scope. An
// eponymous owner's TypeClass spelling can omit a class leaf, so it is not a
// substitute. The defining owner supplies these independent Source origins.
template<class Owner> compat::RetainedString inspectorNestedTypeScope(bool qualified) {
    using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<Owner>;
    if (qualified) {
        if constexpr (requires { Names::nestedQualifiedScope(); }) return Names::nestedQualifiedScope();
        else if constexpr (requires { Names::nestedQualifiedScope; })
            return compat::RetainedString::fromStaticLiteral(Names::nestedQualifiedScope);
    } else {
        if constexpr (requires { Names::nestedUnqualifiedScope(); }) return Names::nestedUnqualifiedScope();
        else if constexpr (requires { Names::nestedUnqualifiedScope; })
            return compat::RetainedString::fromStaticLiteral(Names::nestedUnqualifiedScope);
    }
    throw std::logic_error("Missing Source inspector lexical type-scope metadata");
}

template<class Owner, class Descriptor> compat::RetainedString inspectorNestedName(
    bool classInfo, bool qualified = true) {
    const auto owner = classInfo ? inspectorDeclaredClassName<Owner>() : inspectorNestedTypeScope<Owner>(qualified);
    const auto identifier = Descriptor::id.view();
    return inspectorNameBytes([&] {
        return std::string(owner.view()) + ".SharedValue_" + std::string(identifier);
    });
}

// Object defaults from object.d, Digital Mars 2000-2011, BSL-1.0;
// complete terms: SDK core/licenses/druntime-associative-array.txt.
template<class T> std::size_t inspectorObjectHash(T* object) noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(object));
    return address ^ (address >> 4);
}

[[noreturn]] inline void inspectorObjectComparison(const std::type_info& type) {
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(type);
    const auto message = inspectorNameBytes([&] { return "need opCmp for class " + std::string(name.view()); });
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
} // namespace nijigenerate::panels::inspector::detail

namespace nicxlive::core::d_runtime {

template<> struct SourceObjectTypeNames<::nijigenerate::panels::inspector::Inspector> {
    inline static constexpr char className[] = "nijigenerate.panels.inspector.common.Inspector";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.inspector.common.Inspector)";
};

template<> struct SourceTypeSpellings<::nijigenerate::panels::inspector::Inspector> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.inspector.common.Inspector)";
    inline static constexpr char unqualifiedArgument[] = "(Inspector)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.inspector.common.Inspector";
    inline static constexpr char unqualifiedElement[] = "Inspector";
};

// The three actual class-template declarations use the selected printer's
// independent ClassInfo and eponymous-type contexts. No native name is parsed.
#define NG_INSPECTOR_SINGLE_ORIGIN(Declaration) \
    template<class T> struct SourceObjectTypeNames<::nijigenerate::panels::inspector::Declaration<T>> { \
        static RetainedString className() { \
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument); \
            return ::nijigenerate::panels::inspector::detail::inspectorSingleDeclaration( \
                #Declaration, argument.view(), true, true); \
        } \
        static RetainedString nestedQualifiedScope() { \
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument); \
            return ::nijigenerate::panels::inspector::detail::inspectorSingleDeclaration( \
                #Declaration, argument.view(), true, true); \
        } \
        static RetainedString nestedUnqualifiedScope() { \
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument); \
            return ::nijigenerate::panels::inspector::detail::inspectorSingleDeclaration( \
                #Declaration, argument.view(), false, true); \
        } \
        static RetainedString element(bool qualified) { \
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument); \
            return ::nijigenerate::panels::inspector::detail::inspectorSingleDeclaration( \
                #Declaration, argument.view(), qualified, false); \
        } \
        static RetainedString templateArgumentName() { \
            return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(element(true)); \
        } \
    }; \
    template<class T> struct SourceTypeSpellings<::nijigenerate::panels::inspector::Declaration<T>> { \
        inline static constexpr bool isNativeObject = true; \
        static SourceTypeSpellingNames get() { \
            using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::Declaration<T>>; \
            const auto qualified = Names::element(true); \
            const auto unqualified = Names::element(false); \
            return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified), \
                ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), \
                qualified, unqualified}; \
        } \
    }

NG_INSPECTOR_SINGLE_ORIGIN(TypedInspector);
NG_INSPECTOR_SINGLE_ORIGIN(InspectorHolder);
#undef NG_INSPECTOR_SINGLE_ORIGIN

template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires std::derived_from<T, ::nijigenerate::panels::inspector::Node> &&
        (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
         Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::BaseInspector<Mode, T>> {
    static RetainedString className() {
        const auto element = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
        return ::nijigenerate::panels::inspector::detail::inspectorBaseDeclaration(Mode, element.view(), true, true);
    }
    static RetainedString nestedQualifiedScope() { return typeScope(true, true); }
    static RetainedString nestedUnqualifiedScope() { return typeScope(false, true); }
    static RetainedString element(bool qualified) { return typeScope(qualified, false); }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(element(true));
    }
private:
    static RetainedString typeScope(bool qualified, bool classLeaf) {
        const auto element = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
        return ::nijigenerate::panels::inspector::detail::inspectorBaseDeclaration(
            Mode, element.view(), qualified, classLeaf);
    }
};

template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires std::derived_from<T, ::nijigenerate::panels::inspector::Node> &&
        (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
         Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::BaseInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::BaseInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};

template<class Owner, class Descriptor, bool Deformation>
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::InspectorSharedValue<Owner, Descriptor, Deformation>> {
    static RetainedString className() {
        return ::nijigenerate::panels::inspector::detail::inspectorNestedName<Owner, Descriptor>(true);
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(
            ::nijigenerate::panels::inspector::detail::inspectorNestedName<Owner, Descriptor>(false));
    }
};

template<class Owner, class Descriptor, bool Deformation>
struct SourceTypeSpellings<::nijigenerate::panels::inspector::InspectorSharedValue<Owner, Descriptor, Deformation>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        const auto qualified = ::nijigenerate::panels::inspector::detail::inspectorNestedName<Owner, Descriptor>(false);
        const auto unqualified = ::nijigenerate::panels::inspector::detail::inspectorNestedName<Owner, Descriptor>(false, false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::panels::inspector {

template<class T> compat::RetainedString TypedInspector<T>::toString() {
    const auto owner = this->retainSelf();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
template<class T> std::size_t TypedInspector<T>::toHash() { return detail::inspectorObjectHash(this); }
template<class T> bool TypedInspector<T>::opEquals(std::shared_ptr<Inspector> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
template<class T> int TypedInspector<T>::opCmp(std::shared_ptr<Inspector>) {
    const auto owner = this->retainSelf();
    prepareSourceClassInfo();
    detail::inspectorObjectComparison(typeid(*this));
}
template<class T> void TypedInspector<T>::prepareSourceClassInfo() {
    detail::prepareInspectorClassRecord<TypedInspector<T>>();
}

template<class T> requires std::derived_from<T, Node>
void BaseInspector<ModelEditSubMode::Layout, T>::prepareSourceClassInfo() {
    TypedInspector<Node>::prepareSourceClassInfo();
    detail::prepareInspectorClassRecord<BaseInspector<ModelEditSubMode::Layout, T>>();
}
template<class T> requires std::derived_from<T, Node>
void BaseInspector<ModelEditSubMode::Deform, T>::prepareSourceClassInfo() {
    TypedInspector<Node>::prepareSourceClassInfo();
    detail::prepareInspectorClassRecord<BaseInspector<ModelEditSubMode::Deform, T>>();
}
template<class T> void InspectorHolder<T>::prepareSourceClassInfo() {
    TypedInspector<T>::prepareSourceClassInfo();
    detail::prepareInspectorClassRecord<InspectorHolder<T>>();
}

template<class Owner, class Descriptor, bool Deformation>
compat::RetainedString InspectorSharedValue<Owner, Descriptor, Deformation>::toString() {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
template<class Owner, class Descriptor, bool Deformation>
std::size_t InspectorSharedValue<Owner, Descriptor, Deformation>::toHash() {
    return detail::inspectorObjectHash(this);
}
template<class Owner, class Descriptor, bool Deformation>
bool InspectorSharedValue<Owner, Descriptor, Deformation>::opEquals(std::shared_ptr<InspectorSharedValue> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
template<class Owner, class Descriptor, bool Deformation>
int InspectorSharedValue<Owner, Descriptor, Deformation>::opCmp(std::shared_ptr<InspectorSharedValue>) {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    detail::inspectorObjectComparison(typeid(*this));
}
template<class Owner, class Descriptor, bool Deformation>
void InspectorSharedValue<Owner, Descriptor, Deformation>::prepareSourceClassInfo() {
    detail::prepareInspectorClassRecord<InspectorSharedValue>();
}
} // namespace nijigenerate::panels::inspector
