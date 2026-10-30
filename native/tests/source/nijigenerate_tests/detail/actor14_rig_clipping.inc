// Existing regression.d1573-1653; include inside regression::detail after helpers.
void testRigClippingVisibility() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::core::settings::SettingsJsonValue;
    using nijigenerate::core::settings::SettingsJsonObject;
    using nijigenerate::core::settings::SettingsJsonArray;
    using nijigenerate::core::settings::SettingsString;
    using nijigenerate::autorig::deterministic::native::ngRigMaterialIsActive;
    using namespace nijigenerate::autorig::deterministic::evidence;
    using nicxlive::core::nodes::MaskBinding;
    using nicxlive::core::nodes::MaskingMode;
    using nicxlive::core::nodes::DynamicComposite;
    using nicxlive::core::nodes::Composite;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::ConvertToCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto object = [](std::initializer_list<std::pair<SettingsString, SettingsJsonValue>> fields) {
        SettingsJsonObject values;
        for (const auto& field : fields) values[field.first] = field.second;
        return SettingsJsonValue(values);
    };
    resetCase();
    auto base = newMeshPart(text("clipping-base"));
    auto clipped = newMeshPart(text("clipped"));
    clipped->masks = SharedArray<MaskBinding>{MaskBinding{base->getUUID(),
        std::static_pointer_cast<nicxlive::core::nodes::Drawable>(base), MaskingMode::Mask}};
    check(ngRigMaterialIsActive(clipped), "Visible clipping chain must remain active");
    base->setEnabled(false);
    check(!ngRigMaterialIsActive(clipped), "A hidden clipping base must disable its clipped material");
    base->setEnabled(true);
    base->opacity = 0;
    check(!ngRigMaterialIsActive(clipped), "A transparent clipping base must disable its clipped material");
    base->opacity = 1;
    base->masks = SharedArray<MaskBinding>{MaskBinding{clipped->getUUID(),
        std::static_pointer_cast<nicxlive::core::nodes::Drawable>(clipped), MaskingMode::Mask}};
    check(!ngRigMaterialIsActive(clipped), "Cyclic clipping chains must not recurse indefinitely");
    SettingsJsonArray materials{
        object({{text("uuid"), SettingsJsonValue(3)}, {text("receiver"), SettingsJsonValue(2)},
            {text("active"), SettingsJsonValue(true)}}),
        object({{text("uuid"), SettingsJsonValue(1)}, {text("active"), SettingsJsonValue(false)}}),
        object({{text("uuid"), SettingsJsonValue(2)}, {text("receiver"), SettingsJsonValue(1)},
            {text("active"), SettingsJsonValue(true)}}),
        object({{text("uuid"), SettingsJsonValue(4)}, {text("active"), SettingsJsonValue(true)}})};
    ngRigPropagateClippingVisibility(materials);
    check(!materials[0].sourceIndexValue("active").as_bool() &&
        !materials[2].sourceIndexValue("active").as_bool() && materials[3].sourceIndexValue("active").as_bool(),
        "Empty base alpha must propagate through an unordered clipping chain");
    SettingsJsonArray grouped{
        object({{text("uuid"), SettingsJsonValue(11)}, {text("active"), SettingsJsonValue(true)},
            {text("source_order"), SettingsJsonValue(2)},
            {text("ancestor_ids"), SettingsJsonValue(SettingsJsonArray{SettingsJsonValue(10)})}}),
        object({{text("uuid"), SettingsJsonValue(12)}, {text("active"), SettingsJsonValue(true)},
            {text("source_order"), SettingsJsonValue(1)},
            {text("ancestor_ids"), SettingsJsonValue(SettingsJsonArray{SettingsJsonValue(10)})}}),
        object({{text("uuid"), SettingsJsonValue(13)}, {text("active"), SettingsJsonValue(true)},
            {text("source_order"), SettingsJsonValue(3)}, {text("receiver"), SettingsJsonValue(10)}})};
    SettingsJsonArray receiverGroups{object({{text("uuid"), SettingsJsonValue(10)}})};
    ngRigResolveGroupReceivers(grouped, receiverGroups);
    check(grouped[2].sourceIndexValue("receiver").as_int64() == 12 &&
        grouped[2].sourceIndexValue("mask_receiver").as_int64() == 10,
        "Group receiver classification must resolve to a source material and retain the native mask surface");
    grouped[1].sourceIndexAssignSlot(text("static")) = SettingsJsonValue(true);
    grouped[0].sourceIndexAssignSlot(text("static")) = SettingsJsonValue(false);
    ngRigResolveGroupReceivers(grouped, receiverGroups, true);
    check(grouped[2].sourceIndexValue("receiver").as_int64() == 11,
        "Group receiver selection must skip a descendant classified as static");
    SettingsJsonArray staticChain{
        object({{text("uuid"), SettingsJsonValue(3)}, {text("receiver"), SettingsJsonValue(2)},
            {text("active"), SettingsJsonValue(true)}}),
        object({{text("uuid"), SettingsJsonValue(1)}, {text("static"), SettingsJsonValue(true)},
            {text("active"), SettingsJsonValue(true)}}),
        object({{text("uuid"), SettingsJsonValue(2)}, {text("receiver"), SettingsJsonValue(1)},
            {text("active"), SettingsJsonValue(true)}})};
    ngRigPropagateClippingStatic(staticChain, {});
    check(staticChain[0].sourceIndexValue("static").as_bool() && staticChain[2].sourceIndexValue("static").as_bool(),
        "Static direct receivers must propagate through an unordered clipping chain");
    grouped[1].sourceIndexAssignSlot(text("static")) = SettingsJsonValue(false);
    grouped[1].sourceIndexAssignSlot(text("receiver")) = SettingsJsonValue(1);
    grouped.append(staticChain[1]);
    ngRigPropagateClippingStatic(grouped, receiverGroups);
    check(grouped[1].sourceIndexValue("static").as_bool() && grouped[2].sourceIndexValue("receiver").as_int64() == 11 &&
        grouped[2].sourceObjectRef().if_contains("static") == nullptr,
        "A folder alias must fall back when its first descendant has a static direct receiver");
    grouped[0].sourceIndexAssignSlot(text("active")) = SettingsJsonValue(false);
    grouped[1].sourceIndexAssignSlot(text("active")) = SettingsJsonValue(false);
    grouped[2].sourceIndexAssignSlot(text("receiver")) = SettingsJsonValue(10);
    ngRigResolveGroupReceivers(grouped, receiverGroups);
    check(!grouped[2].sourceIndexValue("active").as_bool(),
        "An empty receiver group must disable its clipped material");
    base->masks = {};
    auto group = DynamicComposite::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    group->setName(NullableString::fromStaticLiteral("clip-render-surface"));
    group->masks = SharedArray<MaskBinding>{MaskBinding{base->getUUID(),
        std::static_pointer_cast<nicxlive::core::nodes::Drawable>(base), MaskingMode::Mask}};
    clipped->reparent(group, 0);
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    context->puppet(nijigenerate::project::incActivePuppet());
    context->nodes(Context::Nodes{group});
    auto converted = constructSourceCommand<ConvertToCommand>(text("Composite"))->run(context);
    check(ngRequireObject(converted)->succeeded, "A clipping surface must convert directly to Composite");
    auto composite = std::dynamic_pointer_cast<Composite>(converted->created[0]);
    check(composite && composite->getUUID() == group->getUUID() && composite->masks.size() == 1 &&
        composite->masks[0].maskSrcUUID == base->getUUID(),
        "Clipping surface conversion must retain identity and masks");
    base->setEnabled(false);
    check(!ngRigMaterialIsActive(clipped), "Ancestor clipping masks must participate in effective visibility");
}
