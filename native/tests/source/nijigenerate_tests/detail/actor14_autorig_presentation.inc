// Existing regression.d18068-18177; include inside regression::detail after helpers.
void testAutoRigPresentationCommands() {
    using namespace nijigenerate::commands;
    using namespace nijigenerate::commands::viewport::control;
    using namespace nijigenerate::commands::puppet::view;
    using nijigenerate::commands::view::panel::TogglePanelVisibilityCommand;
    using nijigenerate::commands::puppet::edit::UndoCommand;
    using nijigenerate::commands::puppet::tool::GetAutoRigStatusCommand;
    using nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommandScope;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::api::mcp::task::ngMcpSetExternalCommandsBlocked;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ThrowableException;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::Parameter;
    using nicxlive::core::math::Vec2;
    using nijigenerate::viewport::base::Viewport;
    using nijigenerate::viewport::base::MainViewport;
    using nijigenerate::viewport::base::viewportTargetZoom;
    using nijigenerate::viewport::base::viewportTargetPosition;
    using nijigenerate::viewport::depth::viewport::DepthEditViewport;
    using nijigenerate::windows::base::Window;
    using nijigenerate::project::incSelectedNodes;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto sameNodes = [](auto left, auto right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (!nicxlive::core::ngResourceObjectEquals(left[i], right[i])) return false;
        return true;
    };
    resetCase();
    const auto presentation = []<class C>() {
        auto command = constructSourceCommand<C>();
        ngRegisterCommandMeta(command);
        return ngIsPresentationCommand(command);
    };
    check(presentation.template operator()<ResetViewportZoomCommand>(), "zoom reset must remain available");
    check(presentation.template operator()<ResetViewportPositionCommand>(), "pan reset must remain available");
    check(presentation.template operator()<FitViewportToModelCommand>(), "camera fit must remain available");
    check(presentation.template operator()<SetDefaultLayoutCommand>(), "layout must remain available");
    check(presentation.template operator()<TogglePanelVisibilityCommand>(), "panel visibility must remain available");
    check(presentation.template operator()<CaptureLiveScreenshotCommand>(), "capture must remain available");
    check(!ngIsPresentationCommand(constructSourceCommand<UndoCommand>()), "undo must not enter the running transaction");
    check(!ngIsPresentationCommand(constructSourceCommand<ResetParametersCommand>()), "parameters must remain protected");
    check(!ngIsPresentationCommand(constructSourceCommand<TogglePhysicsCommand>()), "AutoRig must own physics driver state");
    check(!ngIsPresentationCommand(constructSourceCommand<OpenAutomeshBatchingCommand>()), "competing mesh edits must be blocked");
    viewportTargetZoom = 0.5f;
    viewportTargetPosition = Vec2{20, 30};
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto zoomReset = constructSourceCommand<ResetViewportZoomCommand>();
    auto positionReset = constructSourceCommand<ResetViewportPositionCommand>();
    ngMcpSetExternalCommandsBlocked(true);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto edit = constructSourceCommand<ResetParametersCommand>();
        check(!ngCommandAllowedInCurrentContext(edit), "unannotated commands must be blocked at the shared gate");
        check(!ngRequireObject(ngRunCommand(edit, ctx))->succeeded, "direct command dispatch must also reject editing");
        ngRunAutoRigEditorAction([edit] {
            require(ngCommandAllowedInCurrentContext(edit), RetainedString::fromStaticLiteral(
                "internal AutoRig actions must retain editing access"));
        });
        check(!ngCommandAllowedInCurrentContext(edit), "internal editing access must not escape the callback");
        try {
            ngRunAutoRigEditorAction([] {
                throw ThrowableException(RetainedString::fromStaticLiteral("expected boundary failure"),
                    RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 18112);
            });
            check(false, "boundary failure must propagate");
        } catch (const ThrowableException& error) {
            check(error.msg.view() == "expected boundary failure", "unexpected boundary error");
        }
        check(!ngCommandAllowedInCurrentContext(edit), "internal editing access must close on failure");
        auto statusCommand = constructSourceCommand<GetAutoRigStatusCommand>();
        ngRegisterCommandMeta(statusCommand);
        auto dialogScope = ngPushCommandScope(ngCommandScope<PsdDepthDialogCommandScope>());
        check(ngCommandAllowedInCurrentContext(statusCommand), "status polling must survive a modal editing scope");
        check(ngRequireObject(ngRunCommand(zoomReset, ctx))->succeeded, "navigation must survive a modal editing scope");
        check(!ngCommandAllowedInCurrentContext(edit), "modal navigation must not admit editing");
        ngRequireObject(dialogScope)->close();
        auto screenshotContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
        screenshotContext->parameterValue(Vec2{1, 0});
        auto captureCommand = constructSourceCommand<CaptureLiveScreenshotCommand>();
        auto captureResult = ngRunCommand(captureCommand, screenshotContext);
        check(!ngRequireObject(captureResult)->succeeded &&
            captureResult->message.view().find("parameterized screenshots are blocked") != std::string_view::npos,
            "parameterized capture must be rejected before changing the model or touching the renderer");
        class EditingWindowProbe : public Window {
        public:
            EditingWindowProbe() = default;
        protected:
            void onUpdate() override {}
        };
        auto editingWindow = nijigenerate::windows::base::ngCreateWindowDeferred<EditingWindowProbe>(
            [] { return RetainedString::fromStaticLiteral("Editing window probe"); });
        editingWindow->close();
        check(editingWindow->isVisible(), "closing an editing window must be blocked while AutoRig owns history");
        check(ngRequireObject(ngRunCommand(zoomReset, ctx))->succeeded, "camera zoom reset must run while AutoRig owns editing");
        check(ngRequireObject(ngRunCommand(positionReset, ctx))->succeeded, "camera pan reset must run while AutoRig owns editing");
        check(viewportTargetZoom == 1 && viewportTargetPosition.x == 0 && viewportTargetPosition.y == 0,
            "camera resets must apply");
        auto depthView = std::allocate_shared<DepthEditViewport>(SourceAllocator<DepthEditViewport>{});
        ImGuiIO input;
        // These are the Source default boolean slots read by the actual presentation path.
        input.MouseDown[1] = false;
        input.KeyShift = false;
        input.MouseWheel = 1;
        depthView->updatePresentation(&input, nullptr);
        check(depthView->depthCameraState().zoom > 1, "depth navigation must run without an editing tool or editor");
        class SelectionProbe : public Viewport {
        public:
            std::uint32_t notifications{};
            std::uint32_t parameterNotifications{};
            Slice<std::shared_ptr<Node>> selected;
            void selectionChanged(Slice<std::shared_ptr<Node>> nodes) override {
                notifications++;
                selected = nodes;
            }
            void armedParameterChanged(std::shared_ptr<Parameter>) override { parameterNotifications++; }
        };
        auto navigationView = MainViewport::create();
        auto probe = std::allocate_shared<SelectionProbe>(SourceAllocator<SelectionProbe>{});
        navigationView->subView(probe);
        auto node = Node::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        nijigenerate::project::incSelectNode(node);
        navigationView->selectionChanged(Slice<std::shared_ptr<Node>>{node});
        navigationView->armedParameterChanged(nullptr);
        auto browsingSelection = incSelectedNodes();
        auto expectedSelection = Slice<std::shared_ptr<Node>>{node};
        check(sameNodes(browsingSelection, expectedSelection), "browsing selection must remain available during AutoRig");
        check(probe->notifications == 0, "browsing must not initialize editing tools during AutoRig");
        check(probe->parameterNotifications == 0, "AutoRig parameter arming must not initialize browsing edit targets");
        ngMcpSetExternalCommandsBlocked(false);
        editingWindow->close();
        check(!editingWindow->isVisible(), "editing windows must close normally after AutoRig");
        navigationView->selectionChanged(incSelectedNodes());
        navigationView->armedParameterChanged(nullptr);
        check(probe->notifications == 1 && [&] {
            auto actual = probe->selected;
            auto expected = Slice<std::shared_ptr<Node>>{node};
            return sameNodes(actual, expected);
        }(), "selection must reach editing tools after AutoRig");
        check(probe->parameterNotifications == 1, "parameter notifications must resume after AutoRig");
    }, [] { ngMcpSetExternalCommandsBlocked(false); });
}
