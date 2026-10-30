#pragma once
// Whole AutoMeshReflection body from the independent project meta.d.
#include "meta.hpp"
#include "alpha_provider.hpp"
#include "remove_float.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::vertex::automesh::meta {

// Source bindings describe the actual lexical allMembers/overloads in order.
// members() entries contain an attribute tuple and get(Host&), whose result is
// the actual selected member expression. presets() entries contain the actual
// overload attribute tuple and call(Host&), resolving the original member-name
// invocation. No default registry, guessed name or native RTTI is supplied.
// typeName(Host&) returns the genuine dynamic name descriptor, or its final
// component from an existing producer of that same authored expression.
// hasPresetName/hasPostParamWrite/hasAlphaPreview are the source hasMember
// results; selected operations must compile even when a runtime branch is false.
template<class Host> struct AMReflectionSource;

// Typed lowering of a real member expression and its original ordered UDAs.
// Accessor can be an actual pointer-to-member or the translated expression
// callable, including a ref-returning property; it never stores a host copy.
template<class Accessor, class Attributes> struct AMReflectionMember {
    Accessor accessor;
    Attributes attributes;
    template<class Host> requires std::is_invocable_v<Accessor&, Host&>
    decltype(auto) get(Host& host) { return std::invoke(accessor, host); }
    template<class Host> requires std::is_invocable_v<const Accessor&, Host&>
    decltype(auto) get(Host& host) const { return std::invoke(accessor, host); }
};
template<class Accessor, class Attributes>
AMReflectionMember(Accessor, Attributes) -> AMReflectionMember<Accessor, Attributes>;

// The invocation is the actual translated This.member(this) expression. It
// must resolve that member name, rather than calling a cached scanned overload.
template<class Invocation, class Attributes> struct AMReflectionPreset {
    Invocation invocation;
    Attributes attributes;
    template<class Host> decltype(auto) call(Host& host) { return std::invoke(invocation, host); }
};
template<class Invocation, class Attributes>
AMReflectionPreset(Invocation, Attributes) -> AMReflectionPreset<Invocation, Attributes>;

// Shared lowering of the actual method bodies. The receiver is the original
// Host itself; this view neither copies its fields nor creates another host.
// Concrete expansions can keep their real direct base and method identities.
template<class Host> class AutoMeshReflectionBody {
    using Source = AMReflectionSource<Host>;
    using String = compat::RetainedString;
    using Value = core::settings::SettingsJsonValue;
    using Object = core::settings::SettingsJsonObject;
    using Array = core::settings::SettingsJsonArray;
    using Text = core::settings::SettingsString;

    struct _AMFloatField {
        String id, label;
        AutoMeshLevel level = AutoMeshLevel::Preset;
        String widget;
        float minV = std::numeric_limits<float>::quiet_NaN();
        float maxV = std::numeric_limits<float>::quiet_NaN();
        float stepV = std::numeric_limits<float>::quiet_NaN();
        float* ptr = nullptr;
    };
    struct _AMArrayField {
        String id, label;
        AutoMeshLevel level = AutoMeshLevel::Preset;
        String widget;
        float minV = std::numeric_limits<float>::quiet_NaN();
        float maxV = std::numeric_limits<float>::quiet_NaN();
        float stepV = std::numeric_limits<float>::quiet_NaN();
        compat::Slice<float>* ptr = nullptr;
    };
    struct _AMEnumField {
        String id, label;
        AutoMeshLevel level = AutoMeshLevel::Preset;
        compat::Slice<String> values;
        String* ptr = nullptr;
    };
    Host& receiver_;
    Host& host() { return receiver_; }
    auto retainHost() {
        if constexpr (requires(Host& value) { Source::retainHost(value); })
            return Source::retainHost(host());
        else if constexpr (requires(Host& value) { value.weak_from_this().lock(); })
            return host().weak_from_this().lock();
        else return std::shared_ptr<void>{}; // Explicit native borrowed receiver.
    }
    template<class Tuple, class Function> static void each(Tuple&& tuple, Function&& function) {
        using T = std::remove_cvref_t<Tuple>;
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (function(std::type_identity<std::tuple_element_t<I, T>>{}, std::get<I>(tuple)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
    compat::Slice<_AMFloatField> _amFloatFields() {
        compat::Slice<_AMFloatField> result;
        auto members = Source::members();
        each(members, [&](auto, auto member) {
            if constexpr (requires { member.get(host()); }) {
                if constexpr (std::is_same_v<std::remove_reference_t<decltype(member.get(host()))>, float>) {
                    _AMFloatField field;
                    each(member.attributes, [&](auto tag, auto attribute) {
                        if constexpr (std::is_same_v<typename decltype(tag)::type, AMParam>) {
                            field.id = attribute.id; field.label = attribute.label;
                            field.widget = attribute.widget; field.level = attribute.level;
                            field.minV = attribute.min; field.maxV = attribute.max; field.stepV = attribute.step;
                        }
                    });
                    if (!field.id.empty()) {
                        field.ptr = std::addressof(member.get(host()));
                        result.append(field);
                    }
                }
            }
        });
        return result;
    }
    compat::Slice<_AMArrayField> _amArrayFields() {
        compat::Slice<_AMArrayField> result;
        auto members = Source::members();
        each(members, [&](auto, auto member) {
            if constexpr (requires { member.get(host()); }) {
                if constexpr (std::is_same_v<std::remove_reference_t<decltype(member.get(host()))>,
                        compat::Slice<float>>) {
                    _AMArrayField field;
                    each(member.attributes, [&](auto tag, auto attribute) {
                        if constexpr (std::is_same_v<typename decltype(tag)::type, AMParam>) {
                            field.id = attribute.id; field.label = attribute.label;
                            field.widget = attribute.widget; field.level = attribute.level;
                        }
                        if constexpr (std::is_same_v<typename decltype(tag)::type, AMArray>) {
                            field.minV = attribute.min; field.maxV = attribute.max; field.stepV = attribute.step;
                        }
                    });
                    if (!field.id.empty()) {
                        field.ptr = std::addressof(member.get(host()));
                        result.append(field);
                    }
                }
            }
        });
        return result;
    }
    compat::Slice<_AMEnumField> _amEnumFields() {
        compat::Slice<_AMEnumField> result;
        auto members = Source::members();
        each(members, [&](auto, auto member) {
            if constexpr (requires { member.get(host()); }) {
                if constexpr (std::is_same_v<std::remove_reference_t<decltype(member.get(host()))>, String>) {
                    _AMEnumField field;
                    each(member.attributes, [&](auto tag, auto attribute) {
                        if constexpr (std::is_same_v<typename decltype(tag)::type, AMParam>) {
                            field.id = attribute.id; field.label = attribute.label; field.level = attribute.level;
                        }
                        if constexpr (std::is_same_v<typename decltype(tag)::type, AMEnum>)
                            field.values = attribute.values;
                    });
                    if (!field.id.empty() && !field.values.empty()) {
                        field.ptr = std::addressof(member.get(host()));
                        result.append(field);
                    }
                }
            }
        });
        return result;
    }
    compat::Slice<String> _amPresetNames() {
        compat::Slice<String> result;
        auto presets = Source::presets();
        each(presets, [&](auto, auto preset) {
            each(preset.attributes, [&](auto tag, auto attribute) {
                if constexpr (std::is_same_v<typename decltype(tag)::type, AMPreset>) result.append(attribute.name);
            });
        });
        return result;
    }
    static bool readFloat(const Value& input, float& value) {
        if (input.is_int64()) value = static_cast<float>(input.as_int64());
        else if (input.is_double()) value = static_cast<float>(input.as_double());
        else return false;
        return value == value;
    }
    void postWrite(String id) {
        if constexpr (Source::hasPostParamWrite) Source::postParamWrite(host(), id);
    }
    static String literal(std::string_view value) { return String::fromStaticView(value); }
    template<class Allocate> static auto allocateReflectionMemory(Allocate allocate) {
        try {
            return allocate();
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    String _uidPrefix() {
        const auto id = host().procId();
        return allocateReflectionMemory([&] {
            std::string result;
            if (!id.empty()) result.assign(id.data(), id.size());
            result += '@';
            result += std::to_string(reinterpret_cast<std::uintptr_t>(std::addressof(host())));
            return String(std::move(result));
        });
    }
    String _wuid(String sub) {
        const auto prefix = _uidPrefix();
        return allocateReflectionMemory([&] {
            std::string result(prefix.view()); result += '/'; result += sub.view();
            return String(std::move(result));
        });
    }
    static String terminated(String text) {
        return allocateReflectionMemory([&] {
            std::string result;
            if (!text.empty()) result.assign(text.data(), text.size());
            result += '\0';
            return String(std::move(result));
        });
    }

public:
    using This = Host;
    explicit AutoMeshReflectionBody(Host& host) : receiver_(host) {}
    AutoMeshReflectionBody(const AutoMeshReflectionBody&) = delete;
    AutoMeshReflectionBody& operator=(const AutoMeshReflectionBody&) = delete;

    String schema() {
        [[maybe_unused]] const auto owner = retainHost();
        Object object;
        const auto name = Source::typeName(host());
        const auto dot = name.view().rfind('.');
        object[Text(literal("type"))] =
            Text(dot == std::string_view::npos ? name : name.subslice(dot + 1, name.size()));
        Array presets;
        for (const auto value : _amPresetNames()) {
            Object preset; preset[Text(literal("name"))] = Text(value); presets.append(Value(preset));
        }
        object[Text(literal("presets"))] = presets;
        Array simple, advanced;
        for (const auto field : _amFloatFields()) {
            Object item;
            item[Text(literal("id"))] = Text(field.id);
            item[Text(literal("label"))] = Text(field.label);
            item[Text(literal("type"))] = Text(literal("float"));
            if (field.minV == field.minV) item[Text(literal("min"))] = static_cast<double>(field.minV);
            if (field.maxV == field.maxV) item[Text(literal("max"))] = static_cast<double>(field.maxV);
            if (field.stepV == field.stepV) item[Text(literal("step"))] = static_cast<double>(field.stepV);
            (field.level == AutoMeshLevel::Simple ? simple : advanced).append(Value(item));
        }
        for (const auto field : _amArrayFields()) {
            Object item;
            item[Text(literal("id"))] = Text(field.id);
            item[Text(literal("label"))] = Text(field.label);
            item[Text(literal("type"))] = Text(literal("float[]"));
            (field.level == AutoMeshLevel::Simple ? simple : advanced).append(Value(item));
        }
        for (const auto field : _amEnumFields()) {
            Object item;
            item[Text(literal("id"))] = Text(field.id);
            item[Text(literal("label"))] = Text(field.label);
            item[Text(literal("type"))] = Text(literal("enum"));
            Array choices; for (const auto choice : field.values) choices.append(Value(Text(choice)));
            item[Text(literal("values"))] = choices;
            (field.level == AutoMeshLevel::Simple ? simple : advanced).append(Value(item));
        }
        object[Text(literal("Simple"))] = simple; object[Text(literal("Advanced"))] = advanced;
        return compat::ngJsonStringify(Value(object));
    }
    String values(String levelName) {
        [[maybe_unused]] const auto owner = retainHost();
        const bool advanced = levelName.view() == "Advanced";
        Object object;
        for (const auto field : _amFloatFields())
            if ((field.level == AutoMeshLevel::Advanced) == advanced)
                object[Text(field.id)] = static_cast<double>(*field.ptr);
        for (const auto field : _amArrayFields()) if ((field.level == AutoMeshLevel::Advanced) == advanced) {
            Array values; const auto source = *field.ptr;
            for (const auto value : source) values.append(Value(static_cast<double>(value)));
            object[Text(field.id)] = values;
        }
        for (const auto field : _amEnumFields()) if ((field.level == AutoMeshLevel::Advanced) == advanced)
            object[Text(field.id)] = Text(field.ptr->empty() ? String::emptyLiteral() : *field.ptr);
        return compat::ngJsonStringify(Value(object));
    }
    bool applyPreset(String name) {
        [[maybe_unused]] const auto owner = retainHost();
        bool applied = false;
        auto presets = Source::presets();
        each(presets, [&](auto, auto preset) {
            each(preset.attributes, [&](auto tag, auto attribute) {
                if constexpr (std::is_same_v<typename decltype(tag)::type, AMPreset>) {
                    if (!applied && name == attribute.name) { preset.call(host()); applied = true; }
                }
            });
        });
        return applied;
    }
    bool writeValues(String levelName, String updatesJson) {
        [[maybe_unused]] const auto owner = retainHost();
        auto updates = parseJSON(updatesJson);
        if (!updates.is_object()) return false;
        const bool advanced = levelName.view() == "Advanced";
        bool any = false;
        for (const auto field : _amFloatFields()) {
            if ((field.level == AutoMeshLevel::Advanced) != advanced) continue;
            const auto* found = updates.as_object().if_contains(field.id.view());
            if (!found) continue;
            const auto value = *found;
            float number;
            if (!readFloat(value, number)) continue;
            if (field.minV == field.minV && number < field.minV) number = field.minV;
            if (field.maxV == field.maxV && number > field.maxV) number = field.maxV;
            *field.ptr = number; any = true; postWrite(field.id);
        }
        for (const auto field : _amArrayFields()) {
            if ((field.level == AutoMeshLevel::Advanced) != advanced) continue;
            const auto* found = updates.as_object().if_contains(field.id.view());
            if (!found) continue;
            const auto value = *found;
            if (!value.is_array()) continue;
            field.ptr->resize(0);
            const auto elements = value.as_array();
            for (const auto element : elements) {
                float number;
                if (readFloat(element, number)) field.ptr->append(number);
            }
            any = true; postWrite(field.id);
        }
        for (const auto field : _amEnumFields()) {
            if ((field.level == AutoMeshLevel::Advanced) != advanced) continue;
            const auto* found = updates.as_object().if_contains(field.id.view());
            if (!found) continue;
            const auto value = *found;
            if (value.is_string()) {
                const auto candidate = value.as_string().retainedString();
                for (const auto choice : field.values) if (choice == candidate) {
                    *field.ptr = candidate; any = true; break;
                }
            }
            postWrite(field.id);
        }
        return any;
    }

    void configure() {
        [[maybe_unused]] const auto owner = retainHost();
        using namespace ::nijigenerate::widgets;
        using core::translation::ngTranslateRetained;
        const auto prefix = _uidPrefix();
        const auto rootText = allocateReflectionMemory([&] {
            return String(std::string(prefix.view()) + ":CONFIGURE_OPTIONS");
        });
        const auto rootId = terminated(rootText);
        ImGui::PushID(rootId.data());
        const bool hasPresets = !_amPresetNames().empty();
        if (hasPresets) {
            ngText(compat::RetainedStringSlice(ngTranslateRetained(literal("Presets"))));
            ImGui::Indent();
            const auto presetList = _amPresetNames();
            String currentPreset;
            if constexpr (Source::hasPresetName) {
                if (Source::presetName(host()).empty()) {
                    const auto normal = literal("Normal parts");
                    bool found = false;
                    for (const auto name : presetList) if (name == normal) {
                        Source::presetName(host()) = normal; found = true; break;
                    }
                    if (!found && !presetList.empty()) Source::presetName(host()) = presetList[0];
                }
                currentPreset = Source::presetName(host());
            } else currentPreset = presetList.empty() ? String::emptyLiteral() : presetList[0];
            const auto comboId = terminated(_wuid(literal("PRESET_COMBO")));
            ImGui::PushID(comboId.data());
            const auto id = terminated(_wuid(literal("PRESETS")));
            const auto preview = core::translation::ngTranslateCString(currentPreset.view());
            if (ImGui::BeginCombo(id.data(), preview.data())) {
                for (const auto name : presetList) {
                    const auto label = core::translation::ngTranslateCString(name.view());
                    if (ImGui::Selectable(label.data())) {
                        host().applyPreset(name);
                        if constexpr (Source::hasPresetName) Source::presetName(host()) = name;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID(); ImGui::Unindent();
        }
        configureLevel(false);
        configureLevel(true);
        ImGui::PopID();
        if constexpr (Source::hasAlphaPreview) {
            ImGui::Separator();
            ngText(compat::RetainedStringSlice(ngTranslateRetained(literal("Alpha Preview"))));
            ImGui::Indent();
            alphaPreviewWidget(Source::alphaPreview(host()), ImVec2(192, 192));
            ImGui::Unindent();
        }
    }

private:
    void configureLevel(bool advanced) {
        using namespace ::nijigenerate::widgets;
        using core::translation::ngTranslateRetained;
        const auto label = ngTranslateRetained(literal(advanced ? "Advanced" : "Simple"));
        const auto suffix = _wuid(literal(advanced ? "CAT_ADVANCED" : "CAT_SIMPLE"));
        const auto id = allocateReflectionMemory([&] {
            std::string joined(label.view()); joined += "###"; joined += suffix.view(); joined += '\0';
            return String(std::move(joined));
        });
        const bool open = ngBeginCategory(id.data(), advanced ? IncCategoryFlags::DefaultClosed : IncCategoryFlags::None);
        if (open) {
            for (const auto field : _amFloatFields()) {
                if (field.level != (advanced ? AutoMeshLevel::Advanced : AutoMeshLevel::Simple)) continue;
                ngText(compat::RetainedStringSlice(ngTranslateRetained(field.label))); ImGui::Indent();
                ImGui::SetNextItemWidth(96);
                const auto format = literal(field.stepV == 1 ? "%.0f" : "%.2f");
                const auto id = terminated(_wuid(field.id)); ImGui::PushID(id.data());
                const bool changed = ngDragFloat(compat::RetainedStringSlice(_wuid(field.id)), field.ptr,
                    field.stepV == field.stepV ? field.stepV : 1,
                    field.minV == field.minV ? field.minV : 0,
                    field.maxV == field.maxV ? field.maxV : 0,
                    compat::RetainedStringSlice(format), ImGuiSliderFlags_NoRoundToFormat);
                ImGui::PopID(); ImGui::Unindent();
                if (changed) postWrite(field.id);
            }
            for (const auto field : _amEnumFields()) {
                if (field.level != (advanced ? AutoMeshLevel::Advanced : AutoMeshLevel::Simple)) continue;
                ngText(compat::RetainedStringSlice(ngTranslateRetained(field.label))); ImGui::Indent();
                const auto current = field.ptr->empty() ? String::emptyLiteral() : *field.ptr;
                const auto preview = terminated(current);
                const auto id = terminated(_wuid(field.id));
                if (ImGui::BeginCombo(id.data(), preview.data())) {
                    for (const auto choice : field.values) {
                        const auto label = terminated(choice);
                        if (ImGui::Selectable(label.data())) { *field.ptr = choice; postWrite(field.id); }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Unindent();
            }
            if (advanced) for (const auto field : _amArrayFields()) configureArray(field);
        }
        ngEndCategory();
    }
    void configureArray(const _AMArrayField& field) {
        using namespace ::nijigenerate::widgets;
        std::int32_t deleteIndex = -1;
        ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(field.label)));
        ImGui::Indent();
        const auto id = terminated(_wuid(field.id)); ImGui::PushID(id.data());
        if (!field.ptr->empty()) {
            const auto source = *field.ptr;
            for (std::size_t i = 0; i < source.size(); ++i) {
                ImGui::SetNextItemWidth(96); ImGui::PushID(static_cast<std::int32_t>(i));
                // Foreach retains its old descriptor, but the authored indexed
                // expression rereads the current field after prior UI callbacks.
                auto current = *field.ptr;
                if (i >= current.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                    i, current.size(), literal("source/nijigenerate/viewport/vertex/automesh/meta.d"), 346);
                ngDragFloat(compat::RetainedStringSlice(literal("value")), current.data() + i,
                    field.stepV == field.stepV ? field.stepV : 0.01,
                    field.minV == field.minV ? field.minV : -1,
                    field.maxV == field.maxV ? field.maxV : 2,
                    compat::RetainedStringSlice(literal("%.2f")), ImGuiSliderFlags_NoRoundToFormat);
                ImGui::SameLine(0, 0);
                if (i == field.ptr->size() - 1) {
                    ngDummy(ImVec2(-52, 32)); ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xEE\xA4\xAE", ImVec2(24, 24))) deleteIndex = static_cast<std::int32_t>(i);
                    ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xEE\x85\x85", ImVec2(24, 24))) field.ptr->append(1.0f);
                } else {
                    ngDummy(ImVec2(-28, 32)); ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xEE\xA4\xAE", ImVec2(24, 24))) deleteIndex = static_cast<std::int32_t>(i);
                }
                ImGui::PopID();
            }
        } else {
            ngDummy(ImVec2(-28, 24)); ImGui::SameLine(0, 0);
            if (ngButtonColored("\xEE\x85\x85", ImVec2(24, 24))) field.ptr->append(1.0f);
        }
        if (deleteIndex != -1) *field.ptr = detail::removeFloat(*field.ptr, static_cast<std::uint32_t>(deleteIndex));
        ImGui::PopID(); ImGui::Unindent();
    }
};

// Native mixin transport for a translated lexical Host and its genuine Base.
// Existing concrete source expansions may instead forward their own methods
// to AutoMeshReflectionBody, retaining their original direct base unchanged.
template<class Host, class Base> class AutoMeshReflection : public Base {
    AutoMeshReflectionBody<Host> body() { return AutoMeshReflectionBody<Host>(static_cast<Host&>(*this)); }
public:
    using This = Host;
    using Base::Base;
    AutoMeshReflection() = default;
    AutoMeshReflection(const AutoMeshReflection&) = delete;
    AutoMeshReflection& operator=(const AutoMeshReflection&) = delete;
    virtual compat::RetainedString schema() { return body().schema(); }
    virtual compat::RetainedString values(compat::RetainedString levelName) { return body().values(levelName); }
    virtual bool applyPreset(compat::RetainedString name) { return body().applyPreset(name); }
    virtual bool writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) {
        return body().writeValues(levelName, updatesJson);
    }
    void configure() override { body().configure(); }
};
} // namespace nijigenerate::viewport::vertex::automesh::meta
