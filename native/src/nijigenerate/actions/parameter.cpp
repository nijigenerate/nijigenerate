/*
    Copyright © 2020-2023,2022 Inochi2D Project
    Copyright © 2024 nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
#include "parameter.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::actions::parameter {
// Keep the complete built-in template bodies in the canonical compilation unit.
template class ParameterAddRemoveAction<true>;
template class ParameterAddRemoveAction<false>;
template class ParameterGroupAddRemoveAction<true>;
template class ParameterGroupAddRemoveAction<false>;
template class ParameterValueChangeAction<std::string>;
template class ParameterValueChangeAction<nicxlive::core::common::NullableString>;
template class ParameterValueChangeAction<float>;
template class ParameterValueChangeAction<bool>;

ParameterNameChangeAction::ParameterNameChangeAction(ParameterRef parameter,
        nicxlive::core::common::NullableString before, nicxlive::core::common::NullableString after) {
    compat::ngRequireObject(parameter);
    initialize(compat::RetainedString::fromStaticLiteral("name"), parameter,
               std::move(before), std::move(after),
               std::shared_ptr<nicxlive::core::common::NullableString>(parameter, &parameter->name));
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterNameChangeAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterNameChangeAction"));
}
void ParameterNameChangeAction::rollback() {
    const auto owner = weak_from_this().lock();
    ParameterValueChangeAction::rollback();
    compat::ngRequireObject(self)->makeIndexable();
}
void ParameterNameChangeAction::redo() {
    const auto owner = weak_from_this().lock();
    ParameterValueChangeAction::redo();
    compat::ngRequireObject(self)->makeIndexable();
}
bool ParameterNameChangeAction::canMerge(ActionPtr other) {
    const auto change = std::dynamic_pointer_cast<ParameterNameChangeAction>(other);
    return change && self == change->self;
}

ParameterMoveAction::ParameterMoveAction(ParameterRef parameter, GroupRef before, GroupRef after) {
    initialize(parameter, before, after);
}
void ParameterMoveAction::initialize(const ParameterRef& parameter, const GroupRef& before, const GroupRef& after) {
    param = std::dynamic_pointer_cast<ext::ExParameter>(parameter);
    oldParent = before;
    newParent = after;
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterMoveAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterMoveAction"));
}
compat::RetainedString ParameterMoveAction::toString() {
    const auto receiver=weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
void ParameterMoveAction::rollback() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    const auto receiver = param;
    if (receiver) receiver->setParent(oldParent);
}
void ParameterMoveAction::redo() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    const auto receiver = param;
    if (receiver) receiver->setParent(newParent);
}
ActionText ParameterMoveAction::describe() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    return detail::describe(compat::RetainedString::fromStaticLiteral("%s moved"), [this] {
        const auto receiver = param;
        return receiver ? receiver->getName() : nicxlive::core::common::NullableString::fromStaticLiteral("");
    });
}
ActionText ParameterMoveAction::describeUndo() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    return detail::describe(compat::RetainedString::fromStaticLiteral("%s move cancelled"), [this] {
        const auto receiver = param;
        return receiver ? receiver->getName() : nicxlive::core::common::NullableString::fromStaticLiteral("");
    });
}

ParameterShapeChangeAction::ParameterShapeChangeAction(ActionText label, ParameterRef parameter)
    : name(label), self(parameter) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterShapeChangeAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterShapeChangeAction"));
    oldState = captureState();
}
compat::RetainedString ParameterShapeChangeAction::toString() {
    const auto receiver=weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
void ParameterShapeChangeAction::updateNewState() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    newState = captureState();
    notifyBindingValuesChanged();
}
void ParameterShapeChangeAction::rollback() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    applyState(oldState);
}
void ParameterShapeChangeAction::redo() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    applyState(newState);
}
ActionText ParameterShapeChangeAction::describe() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s->%s changed"));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName()),
        core::translation::ngFormatArgument(name)};
    return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
ActionText ParameterShapeChangeAction::describeUndo() {
    [[maybe_unused]] const auto actionOwner = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s->%s change cancelled"));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName()),
        core::translation::ngFormatArgument(name)};
    return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
Matrix<float> ParameterShapeChangeAction::dupFloatMatrix(Matrix<float> source) {
    Matrix<float> result;
    result.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto row = source[i];
        result[i] = row.duplicate();
    }
    return result;
}
Matrix<bool> ParameterShapeChangeAction::dupBoolMatrix(Matrix<bool> source) {
    Matrix<bool> result;
    result.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto row = source[i];
        result[i] = row.duplicate();
    }
    return result;
}
runtime::DeformSlot ParameterShapeChangeAction::dupDeformation(runtime::DeformSlot source) {
    auto result = source;
    result.vertexOffsets = source.vertexOffsets.dup();
    return result;
}
Matrix<runtime::DeformSlot> ParameterShapeChangeAction::dupDeformationMatrix(
        Matrix<runtime::DeformSlot> source) {
    Matrix<runtime::DeformSlot> result;
    result.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto row = source[i];
        result[i].resize(row.size());
        for (std::size_t j = 0; j < row.size(); ++j) {
            const auto value = row[j];
            const auto duplicate = dupDeformation(value);
            result[i][j] = duplicate;
        }
    }
    return result;
}
ParameterShapeChangeAction::State ParameterShapeChangeAction::captureState() {
    State state;
    state.min = compat::ngRequireObject(self)->min;
    state.max = compat::ngRequireObject(self)->max;
    {
        const auto receiver=compat::ngRequireObject(self);
        const auto axisPoints=Matrix<float>::retainedView(receiver,
            receiver->axisPoints.data(),receiver->axisPoints.size());
        state.axisPoints=dupFloatMatrix(axisPoints);
    }
    const auto bindings = compat::ngRequireObject(self)->bindings;
    for (const auto& value : bindings) {
        if (auto typed = std::dynamic_pointer_cast<runtime::ValueParameterBinding>(value)) {
            state.valueBindings.append({typed, dupFloatMatrix(typed->values),
                dupBoolMatrix(typed->runtime::ParameterBindingImpl<float>::getIsSet())});
        } else if (auto typed = std::dynamic_pointer_cast<runtime::ParameterParameterBinding>(value)) {
            state.parameterBindings.append({typed, dupFloatMatrix(typed->values),
                dupBoolMatrix(typed->runtime::ParameterBindingImpl<float>::getIsSet())});
        } else if (auto typed = std::dynamic_pointer_cast<runtime::DeformationParameterBinding>(value)) {
            state.deformationBindings.append({typed, dupDeformationMatrix(typed->values),
                dupBoolMatrix(typed->runtime::ParameterBindingImpl<runtime::DeformSlot>::getIsSet())});
        }
    }
    return state;
}
void ParameterShapeChangeAction::applyState(State& state) {
    compat::ngRequireObject(self)->min = state.min;
    compat::ngRequireObject(self)->max = state.max;
    {
        const auto target=self;
        const auto axisPoints=dupFloatMatrix(state.axisPoints);
        const auto receiver=compat::ngRequireObject(target);
        if (axisPoints.size()!=receiver->axisPoints.size()) {
            throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/parameter.d"),531);
        }
        for(std::size_t i=0;i<axisPoints.size();++i)receiver->axisPoints[i]=axisPoints[i];
    }
    const auto values = state.valueBindings;
    for (auto saved : values) {
        compat::ngRequireObject(saved.binding)->values = dupFloatMatrix(saved.values);
        compat::ngRequireObject(saved.binding)->runtime::ParameterBindingImpl<float>::getIsSet() =
            dupBoolMatrix(saved.isSet);
        compat::ngRequireObject(saved.binding)->reInterpolate();
    }
    const auto parameters = state.parameterBindings;
    for (auto saved : parameters) {
        compat::ngRequireObject(saved.binding)->values = dupFloatMatrix(saved.values);
        compat::ngRequireObject(saved.binding)->runtime::ParameterBindingImpl<float>::getIsSet() =
            dupBoolMatrix(saved.isSet);
        compat::ngRequireObject(saved.binding)->reInterpolate();
    }
    const auto deformations = state.deformationBindings;
    for (auto saved : deformations) {
        compat::ngRequireObject(saved.binding)->values = dupDeformationMatrix(saved.values);
        compat::ngRequireObject(saved.binding)->runtime::ParameterBindingImpl<runtime::DeformSlot>::getIsSet() =
            dupBoolMatrix(saved.isSet);
        compat::ngRequireObject(saved.binding)->reInterpolate();
    }
    notifyBindingValuesChanged();
}
void ParameterShapeChangeAction::notifyBindingValuesChanged() {
    const auto bindings = compat::ngRequireObject(self)->bindings;
    for (auto value : bindings) depthboneinvalidation::ngNotifyDepthBoneBindingAllValuesChanged(value);
}

ActionPtr detail::sourceBindingAddMapper(BindingRef& value,ParameterRef& parent) {
    return binding::ngCreateBindingAddRemoveAction<binding::ParameterBindingAddAction>(parent, value);
}
ActionPtr detail::sourceBindingRemoveMapper(BindingRef& value,ParameterRef& parent) {
    return binding::ngCreateBindingAddRemoveAction<binding::ParameterBindingRemoveAction>(parent, value);
}
ActionPtr detail::sourceBindingChangeMapper(BindingRef& value) {
    if (auto typed = std::dynamic_pointer_cast<runtime::ValueParameterBinding>(value)) {
        return binding::ngCreateBindingAllValueActionDeferred<binding::ParameterBindingAllValueChangeAction<float>>(
            [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),typed}; });
    } else if (auto typed = std::dynamic_pointer_cast<runtime::DeformationParameterBinding>(value)) {
        return binding::ngCreateBindingAllValueActionDeferred<
            binding::ParameterBindingAllValueChangeAction<runtime::DeformSlot>>(
                [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),typed}; });
    } else if (auto typed = std::dynamic_pointer_cast<runtime::ParameterParameterBinding>(value)) {
        return binding::ngCreateBindingAllValueActionDeferred<binding::ParameterBindingAllValueChangeAction<float>>(
            [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),typed}; });
    }
    return nullptr;
}
ActionPtr detail::sourceBindingValueChangeMapper(BindingRef& value,int& x,int& y,bool& notify) {
    if (auto typed = std::dynamic_pointer_cast<runtime::ValueParameterBinding>(value)) {
        return binding::ngCreateBindingValueActionDeferred<
            binding::ParameterBindingValueChangeAction<float, runtime::ValueParameterBinding>>(
                [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),
                    typed,x,y,binding::BindingUpdate{},notify}; });
    } else if (auto typed = std::dynamic_pointer_cast<runtime::DeformationParameterBinding>(value)) {
        return binding::ngCreateBindingValueActionDeferred<
            binding::ParameterBindingValueChangeAction<runtime::DeformSlot, runtime::DeformationParameterBinding>>(
                [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),
                    typed,x,y,binding::BindingUpdate{},notify}; });
    } else if (auto typed = std::dynamic_pointer_cast<runtime::ParameterParameterBinding>(value)) {
        return binding::ngCreateBindingValueActionDeferred<
            binding::ParameterBindingValueChangeAction<float, runtime::ParameterParameterBinding>>(
                [&] { return std::tuple{compat::RetainedString(typed->getNameDescriptor()),
                    typed,x,y,binding::BindingUpdate{},notify}; });
    }
    return nullptr;
}
ActionPtr ngBindingAddMapper(BindingRef value,ParameterRef parent) {
    return detail::sourceBindingAddMapper(value,parent);
}
ActionPtr ngBindingRemoveMapper(BindingRef value,ParameterRef parent) {
    return detail::sourceBindingRemoveMapper(value,parent);
}
ActionPtr ngBindingChangeMapper(BindingRef value) {
    return detail::sourceBindingChangeMapper(value);
}
ActionPtr ngBindingValueChangeMapper(BindingRef value,int x,int y,bool notify) {
    return detail::sourceBindingValueChangeMapper(value,x,y,notify);
}
} // namespace nijigenerate::actions::parameter
