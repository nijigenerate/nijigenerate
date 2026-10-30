#include "dynamic.hpp"
#include "detail/dynamic_result.hpp"
#include "detail/dynamic_apply_environment.hpp"
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/viewport/vertex/automesh/common.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <atomic>
#include <array>
#include <thread>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <new>
#include <source_location>
#include <utility>
#include <imgui.h>

namespace nijigenerate::commands::automesh::dynamic {
namespace {
using namespace detail;
using namespace viewport::vertex;
namespace alpha=viewport::vertex::automesh::common;
namespace mesh=viewport::common::mesh;
using String=compat::RetainedString;
using Result=std::shared_ptr<CommandResult>;
using Target=Processor::Target;
using Mesh=Processor::Mesh;
using api::mcp::task::Action;
using api::mcp::task::ngMcpEnqueueAction;
template<class T, class... Args> std::shared_ptr<T> create(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
template<class Operation> auto allocateBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct Job {
    std::jthread worker;
    std::shared_ptr<AutoMeshProgressState> progress;
    std::atomic<bool> workerExited{false};
};
struct PublishWorkerExit {
    std::shared_ptr<Job> job;
    ~PublishWorkerExit() noexcept { job->workerExited.store(true,std::memory_order_release); }
};
std::vector<std::shared_ptr<Job>> jobs;
bool shuttingDown=false;
template<class... Args> String format(std::string_view pattern,Args&&...values) {
    const std::array args{core::translation::ngFormatArgument(std::forward<Args>(values))...};
    auto text = core::translation::ngFormatTyped(pattern,args);
    return allocateBytes([&] { return String(std::move(text)); });
}
template<std::size_t N> String tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(String::fromStaticLiteral(text));
}
void retire(const std::shared_ptr<Job>& job) {
    if(job->worker.joinable()) job->worker.join();
    std::erase(jobs,job);
}
}
namespace detail {
Result runApply(std::shared_ptr<Command> self,std::shared_ptr<Context> context,
        std::shared_ptr<Processor> explicitProcessor,ParentCanceled parentCanceled,
        bool (*matches)(const std::shared_ptr<Processor>&)) {
    if(!core::thread_context::ngIsMainThreadOrUnattached()) {
        return api::mcp::task::ngRunInMainThread<Result>(api::mcp::task::MainThreadAction<Result>(
            [self,context] { return ngRunCommand(self,context); }));
    }
    if(shuttingDown) throw std::logic_error("AutoMesh worker submission after shutdown");
    if(!compat::ngRequireObject(self)->runnable(context))
        return CommandResult::opCall(false,String::fromStaticLiteral("No drawable nodes"));
    auto chosen=explicitProcessor;
    if(chosen && !matches(chosen))
        return CommandResult::opCall(false,String::fromStaticLiteral("AutoMesh processor type mismatch"));
    if(!chosen) for(const auto processor:ngAutoMeshProcessors()) if(matches(processor)) { chosen=processor; break; }
    if(!chosen) return CommandResult::opCall(false,String::fromStaticLiteral("AutoMesh processor missing"));
    const auto receiver=compat::ngRequireObject(context);
    auto nodes=receiver->hasNodes()?receiver->nodes():project::incSelectedNodes();
    compat::Slice<Target> targets;
    compat::Slice<Mesh> meshes;
    for(const auto node:nodes) if(auto target=std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) {
        Mesh value;
        if(auto drawable=std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target)) {
            auto& sourceMesh = drawable->nicxlive::core::nodes::Drawable::getMesh();
            const auto sourceMeshOwner = drawable->mesh;
            value=mesh::IncMesh::create(sourceMesh,sourceMeshOwner);
        }
        else value=mesh::ngCreateIncMesh(target->getVertices());
        targets.append(target); meshes.append(value);
    }
    if(targets.empty()) return CommandResult::opCall(false,String::fromStaticLiteral("No deformable targets"));
    auto inputs=create<alpha::AlphaInputCache>();
    for(const auto target:targets) {
        const auto id=compat::ngRequireObject(target)->getUUID();
        const auto input=alpha::getAlphaInput(target);
        (*inputs)[id]=input;
    }
    auto result=create<AutoMeshApplyResult>();
    auto progress=create<AutoMeshProgressState>(targets.size(),parentCanceled);
    auto name=chosen->displayName();
    const auto popupId=widgets::NotificationPopup::instance()->popup([progress,name](ImGuiIO*) {
        std::size_t done,total; compat::RetainedStringSlice current; bool canceled;
        progress->snapshot(done,total,current,canceled);
        const float ratio=total>0?static_cast<float>(done)/static_cast<float>(total):0.f;
        const auto title=current.empty()?format(tr("AutoMesh: %s").view(),name.view()):
            format(tr("AutoMesh: %s - %s").view(),name.view(),current.view());
        const compat::SourceCString terminated{compat::RetainedStringSlice(title)};
        ImGui::Text(terminated.data());
        ImGui::ProgressBar(ratio,ImVec2(320,0));
        ImGui::SameLine(0,8);
        if(canceled) {
            const auto label=core::translation::ngTranslateCString("Canceling...");
            ImGui::Text(label.data());
        }
        else if(widgets::ngButtonColored(core::translation::ngTranslateCString("Cancel").data(),ImVec2(96,24)))
            progress->requestCancel();
    },-1.f);
    auto job=create<Job>(); job->progress=progress;
    allocateBytes([&] { jobs.push_back(job); });
    try {
        job->worker=std::jthread([job,chosen,targets,meshes,inputs,result,progress,popupId] {
            core::thread_context::ManagedThreadScope threadScope;
            const PublishWorkerExit publishExit{job};
            utils::crashdump::installNativeCrashDumpThreadHandler();
            String workerError;
            auto environment=create<AutoMeshApplyEnvironment>();
            Processor::Callback callback=[environment,progress](Target target,Mesh value) {
                if(!value) {
                    progress->beginTarget(String(compat::ngRequireObject(target)->getName()));
                    return !progress->canceled();
                }
                ngMcpEnqueueAction(Action([environment,progress,target,value] {
                    if(!environment->currentApplyError().empty()) return;
                    compat::ngRunWithScopeExit([&] {
                        if(progress->canceled()) return;
                        if(value->vertices.empty()) {
                            environment->recordApplyError(format(tr("AutoMesh generated empty mesh for %s").view(),
                                compat::ngRequireObject(target)->getName()));
                            return;
                        }
                        const auto drawable=std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target);
                        if(drawable && value->vertices.size()<3) {
                            environment->recordApplyError(format(tr("AutoMesh generated too few drawable vertices for %s").view(),
                                compat::ngRequireObject(target)->getName()));
                            return;
                        }
                        String message;
                        environment->ensureActionGroupOpen();
                        if(drawable) {
                            if(!ngApplyDrawableMeshFromCommand(drawable,value,message)) {
                                environment->recordApplyError(message); return;
                            }
                        } else if(!ngApplyDeformableVerticesFromCommand(target,mesh::ngMeshPositions(value),message)) {
                            environment->recordApplyError(message); return;
                        }
                        environment->incrementApplied();
                    }, [&] { progress->completeTarget(); });
                }));
                return !progress->canceled();
            };
            try {
                alpha::setAutoMeshAlphaInputCache(inputs.get());
                compat::ngRunWithScopeExit([&] {
                    // FiberBase's real entry stack retains its Fiber until TERM.
                    // Preserve that owner when cancellation abandons a HOLD stack;
                    // destroying an automatic Boost fiber here would add an unwind
                    // and run processor scope exits absent from the original.
                    auto entryOwner=create<std::weak_ptr<core::tasks::TaskFiber>>();
                    auto fiber=create<core::tasks::TaskFiber>([chosen,targets,meshes,callback,entryOwner] {
                        [[maybe_unused]] const auto fiberKeepAlive=compat::ngRequireObject(entryOwner->lock());
                        auto backgroundTargets=targets;
                        auto backgroundMeshes=meshes.duplicate();
                        chosen->autoMesh(backgroundTargets,backgroundMeshes,false,0,false,0,callback);
                    });
                    *entryOwner=fiber;
                    while(fiber->state()!=core::tasks::TaskFiber::State::term) {
                        if(progress->canceled()) break;
                        fiber->call();
                    }
                }, [&] { alpha::clearAutoMeshAlphaInputCache(inputs.get()); });
            } catch(const compat::Throwable& error) { workerError=error.msg; }
            if(!workerError.empty()) {
                ngMcpEnqueueAction(Action([job,result,popupId,workerError] {
                    compat::ngRunWithScopeExit([&] {
                        widgets::NotificationPopup::instance()->close(popupId);
                        result->complete(CommandResult::opCall(false,workerError));
                    }, [&] { retire(job); });
                }));
                return;
            }
            ngMcpEnqueueAction(Action([job,result,progress,popupId,environment] {
                compat::ngRunWithScopeExit([&] {
                    compat::ngRunWithScopeExit([&] {
                        compat::ngRunWithScopeExit([&] {
                            const auto error=environment->currentApplyError();
                            const auto applied=environment->appliedCount();
                            if(!error.empty()) result->complete(CommandResult::opCall(false,error.retainedString()));
                            else if(progress->canceled())
                                result->complete(CommandResult::opCall(false,String::fromStaticLiteral("AutoMesh canceled")));
                            else if(applied==0) result->complete(CommandResult::opCall(false,
                                String::fromStaticLiteral("AutoMesh generated no applicable meshes")));
                            else result->complete(CommandResult::opCall(true));
                        }, [&] { environment->closeActionGroup(); });
                    }, [&] { widgets::NotificationPopup::instance()->close(popupId); });
                }, [&] { retire(job); });
            }));
        });
    } catch(...) { std::erase(jobs,job); throw; }
    return result;
}
}
void ngShutdownAutoMeshJobs() {
    if(!core::thread_context::ngIsMainThread()) throw std::logic_error("AutoMesh shutdown requires UI thread");
    shuttingDown=true;
    for(const auto& job:jobs) job->progress->requestCancel();
    while(!jobs.empty()) {
        api::mcp::task::ngMcpProcessQueue();
        bool retired=false;
        for(std::size_t index=0;index<jobs.size();) {
            const auto job=jobs[index];
            if(job->workerExited.load(std::memory_order_acquire)) { retire(job); retired=true; }
            else ++index;
        }
        // A released terminal publication means the worker has already queued
        // its final action. Drain it even when the native job list became empty.
        // If an earlier queue failure discarded that closure, only join/retire;
        // do not invent/replay application or Undo callbacks.
        if(retired) api::mcp::task::ngMcpProcessQueue();
        // UI completions retire their workers only after all earlier apply actions.
        if(!jobs.empty()) std::this_thread::yield();
    }
}
}
