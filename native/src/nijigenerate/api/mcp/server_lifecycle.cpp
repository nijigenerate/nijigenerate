#include "server.hpp"
#include "server_registration.hpp"
#include "http_transport.hpp"
#include "task.hpp"
#include "server_log.hpp"
#include "server_state.hpp"
#include "server_internal.hpp"
#include <nijigenerate/commands/package.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <system_error>
#include <thread>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nijigenerate::api::mcp::server {
using providers::mcp::text;
using String=compat::RetainedStringSlice;
using detail::mcpLog;
namespace {
using detail::gServerStarted;
using detail::gServerEnabled;
using detail::gLoadHookRegistered;
using detail::gServerThreadExited;
using detail::gServerPort;
String host=compat::RetainedString::fromStaticLiteral("127.0.0.1");
std::shared_ptr<http_transport::HttpTransport> transport;
std::shared_ptr<http_transport::ExtMCPServer> instance;
void deleteDaemonWorker(std::jthread* thread) {
    if (!thread) return;
    // The source worker is a daemon: releasing its last handle must not join it.
    if (thread->joinable()) thread->detach();
    delete thread;
}
std::unique_ptr<std::jthread, decltype(&deleteDaemonWorker)> worker{nullptr, deleteDaemonWorker};
std::exception_ptr workerFailure;
std::atomic<bool> workerRuntimeRunning{false};
detail::ResourceNotifications notifications;
bool workerRunning() noexcept {
    if (!worker || !worker->joinable()) return false;
#if defined(_WIN32)
    // Match the actual Windows Thread.isRunning provider, not just the body flag.
    DWORD exitCode = 0;
    ::GetExitCodeThread(worker->native_handle(), &exitCode);
    return exitCode == STILL_ACTIVE;
#else
    return workerRuntimeRunning.load();
#endif
}
void joinWorker() {
    try {
        if (worker && worker->joinable()) worker->join();
    } catch (const std::system_error&) {
        throw compat::ThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"));
    }
    if (workerFailure) std::rethrow_exception(workerFailure);
}
void clearReferences() {
    worker.reset();workerFailure={};instance.reset();transport.reset();notifications={};gServerStarted=false;
}
void clearExited() {
    if(!gServerThreadExited.load()||!worker||workerRunning()) return;
    joinWorker();clearReferences();gServerEnabled=false;
}
void start(String newHost,std::uint16_t newPort) {
    clearExited();if(gServerStarted) return;
    task::ngMcpInitTask();gServerStarted=true;gServerEnabled=true;host=newHost;gServerPort=newPort;
    mcpLog("[MCP] starting server host=%s port=%s",host,gServerPort);
    auto currentTransport=http_transport::createHttpTransport(host,gServerPort);
    auto currentServer=detail::createMcpObject<http_transport::ExtMCPServer>(currentTransport,
        compat::RetainedString::fromStaticLiteral("Nijigenerate MCP"),
        compat::RetainedString::fromStaticLiteral("0.0.1"));
    instance=currentServer;transport=currentTransport;
    ngMcpAuthEnabled(core::settings::incSettingsGet<bool>(
        compat::RetainedString::fromStaticLiteral("MCP.authEnabled"),false));
    commands::ngInitAllCommands();
    detail::registerTools(*currentServer);
    detail::registerResources(*currentServer, notifications);
    workerFailure={};
    gServerThreadExited.store(false);
    // Publish the worker object before starting it, as gServerThread = t does.
    try {
        worker.reset(new std::jthread());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    workerRuntimeRunning.store(true, std::memory_order_relaxed);
    try {
        *worker=std::jthread([currentServer] {
            // Thread.isRunning remains true through managed runtime cleanup.
            struct RuntimeExit {
                ~RuntimeExit() {workerRuntimeRunning.store(false, std::memory_order_relaxed);}
            } runtimeExit;
            const auto runServer=[&] {
                struct Exit {~Exit(){gServerThreadExited.store(true);}} exit;
                utils::crashdump::installNativeCrashDumpThreadHandler();
                mcpLog("[MCP] server thread entering start() ...");
                try {currentServer->start();}
                catch(const compat::Throwable& error) {
                    providers::mcp::stdio::standardError().withTextWriter(
                        [&](providers::mcp::stdio::FileTextWriter& writer) {
                            writer.put("MCP server failed: ");
                            error.toString([&writer](std::string_view part) { writer.put(part); });
                            writer.put(static_cast<unsigned char>('\n'));
                        });
                }
                mcpLog("[MCP] server thread exited start()");
            };
            try {
                core::thread_context::ManagedThreadScope managed;
                try {runServer();}
                catch (...) {
                    // Retain user-delegate failure before detaching the managed thread.
                    workerFailure=std::current_exception();
                }
            } catch (...) {
                // A source Thread retains unhandled setup/logging failures for join().
                workerFailure=std::current_exception();
            }
        });
    } catch (const std::system_error&) {
        workerRuntimeRunning.store(false, std::memory_order_relaxed);
        throw compat::ThreadException(compat::RetainedString::fromStaticLiteral("Error creating thread"));
    } catch (const std::bad_alloc&) {
        workerRuntimeRunning.store(false, std::memory_order_relaxed);
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (...) {
        workerRuntimeRunning.store(false, std::memory_order_relaxed);
        throw;
    }
    if(!gLoadHookRegistered) {
        gLoadHookRegistered=true;
        project::incRegisterLoadFunc([](auto) {
            if(notifications.find) {
                const auto notify=notifications.find;
                notify();
            }
            if(notifications.byUuid) {
                const auto notify=notifications.byUuid;
                notify();
            }
            if(notifications.binding) {
                const auto notify=notifications.binding;
                notify();
            }
            mcpLog("[MCP] puppet load detected -> resources changed");
        });
    }
    detail::registerPrompts(*currentServer);
}
}
void ngMcpInit(String requestedHost,std::uint16_t requestedPort){start(requestedHost,requestedPort);}
bool ngMcpIsRunning(){clearExited();return gServerStarted;}
void ngMcpStop() {
    clearExited();if(!gServerStarted) {mcpLog("[MCP] stop requested but server not started");return;}
    gServerEnabled=false;
    if(instance) {
        mcpLog("[MCP] requesting server stop...");
        try {
            compat::ngRequireObject(instance)->stop();
            mcpLog("[MCP] stop() invoked on transport");
            for(unsigned i=0;i<40;++i) {
                const bool transportExited=compat::ngRequireObject(instance)->transportExited();
                const bool running=workerRunning();
                if(transportExited&&!running) {
                    mcpLog("[MCP] transport exited and thread stopped");break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        } catch(const compat::ThrowableException& error) {
            mcpLog("[MCP] stop() threw: %s",error.msg);
        }
    } else mcpLog("[MCP] no server instance to stop");
    bool canClear=true;
    if(instance&&!compat::ngRequireObject(instance)->transportExited()) canClear=false;
    if(workerRunning()) canClear=false;
    if(canClear) {
        if(worker) {
            try {joinWorker();} catch(const compat::ThrowableException&) {}
        }
        clearReferences();
        mcpLog("[MCP] server state cleared (stopped=true)");
    } else mcpLog("[MCP][WARN] stop requested but server did not exit in time; keeping references");
}
void ngMcpApplySettings(bool requestedEnabled,String requestedHost,std::uint16_t requestedPort) {
    clearExited();
    mcpLog("[MCP] apply settings: enabled=%s host=%s port=%s (running=%s h=%s p=%s)",
        requestedEnabled,requestedHost,requestedPort,gServerStarted,host,gServerPort);
    if(!requestedEnabled) {
        if(gServerStarted) {mcpLog("[MCP] settings disabled -> stopping");ngMcpStop();}
        else mcpLog("[MCP] settings disabled and not running -> no-op");return;
    }
    if(!gServerStarted) {mcpLog("[MCP] settings enabled and not running -> start");start(requestedHost,requestedPort);return;}
    if(host.view()!=requestedHost.view()||gServerPort!=requestedPort) {
        mcpLog("[MCP] settings changed host/port -> restart");ngMcpStop();start(requestedHost,requestedPort);
    } else mcpLog("[MCP] settings unchanged and running -> no-op");
}
void ngMcpLoadSettings() {
    using namespace core::settings;
    const auto requestedEnabled=incSettingsGet<bool>(
        compat::RetainedString::fromStaticLiteral("MCP.Enabled"),false);
    const auto requestedHost=incSettingsGet<SettingsString>(
        compat::RetainedString::fromStaticLiteral("MCP.Host"),
        SettingsString(compat::RetainedString::fromStaticLiteral("127.0.0.1")));
    const auto requestedPort=static_cast<std::uint16_t>(incSettingsGet<int>(
        compat::RetainedString::fromStaticLiteral("MCP.Port"),8088));
    mcpLog("[MCP] load settings: enabled=%s host=%s port=%s",requestedEnabled,requestedHost,requestedPort);
    ngMcpApplySettings(requestedEnabled,requestedHost,requestedPort);
}
void ngMcpAuthEnabled(bool value){if(transport) compat::ngRequireObject(transport)->authEnabled(value);}
bool ngMcpAuthEnabled(){return transport?compat::ngRequireObject(transport)->authEnabled():false;}
void ngMcpShutdown() {
    if(!core::thread_context::ngIsMainThread()) throw std::logic_error("MCP shutdown requires UI thread");
    ngMcpStop();
    // Unlike a settings stop, final teardown cannot abandon a worker with queued
    // UI work. Close first, then service retained actions until its owner can join.
    while(worker&&workerRunning()) {
        task::ngMcpProcessQueue();
        std::this_thread::yield();
    }
    if(worker) joinWorker();
    clearReferences();gServerEnabled=false;
    // A canceled request can leave an already-enqueued UI action after its HTTP
    // worker exits. Its retained state must finish before the UI is destroyed.
    task::ngMcpProcessQueue();
}
}
