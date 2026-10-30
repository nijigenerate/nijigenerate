#pragma once

#include <nijigenerate/panels/agent.hpp>
#include <nijigenerate/panels/agent_permission.hpp>
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <common/resource_key_map.hpp>
#include <compat/source_mutex.hpp>
#include <compat/source_condition.hpp>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <limits>
#include <thread>
#include <exception>
#include <string>

namespace nijigenerate::panels::agent {

class AgentPanel : public Panel, public std::enable_shared_from_this<AgentPanel> {
public:
    AgentPanel();
    ~AgentPanel() override;
    void onInit() override;

protected:
    void prepareSourceClassInfo() override;
    void onUpdate() override;

private:
    static AgentString emptyString() { return compat::RetainedString::fromStaticLiteral(""); }
    static constexpr std::size_t kMaxChatLineChars = 200;
    static constexpr std::size_t kMaxLogLineChars = 2000;
    static constexpr std::size_t kMaxLines = 4000;
    static constexpr std::size_t kMaxDrawCharsPerLine = 4000;
    static constexpr std::size_t permissionOptionLabelMaxChars = 15;
    using Strings = compat::Slice<AgentString>;
    using ConstStrings = compat::ConstSlice<AgentString>;
    using Jsons = compat::Slice<AgentJson>;
    using Clock = compat::SourceMonotonicClock;
    using Time = Clock::time_point;
    struct LineHeightCache {
        float wrapW = 0;
        compat::Slice<float> heights;
        compat::Slice<std::size_t> lens;
        std::uint32_t dataVer = std::numeric_limits<std::uint32_t>::max();
        float totalHeight = 0;
    };
    struct LogBlock {
        AgentString role, title;
        Strings lines;
        Jsons payloads;
        AgentJson payloadMerged;
        bool hasPayloadMerged = false;
        AgentString status, toolId;
        LineHeightCache cache;
        std::uint32_t ver = 0;
        std::size_t dirtyFrom = std::numeric_limits<std::size_t>::max();
    };
    struct TrimView { std::size_t start, end; bool trimmed; };
    struct PlanView {
        Strings lines;
        AgentJson payload;
        AgentString status;
        std::uint32_t ver = 0;
    };
    enum class RoleKind { You, Tool, AssistantFinal, AssistantThinking, Error, Other };

    AgentString statusText = compat::RetainedString::fromStaticLiteral("Idle"), usageText;
    AgentJson usagePayload;
    AgentString userText;
    std::uint32_t userTextInputSerial = 0;
    compat::Slice<LogBlock> convoBlocks;
    AgentString pendingRole;
    compat::Slice<std::uint8_t> pendingBuf;
    bool pendingNewline = false, hasPendingRole = false;
    bool convoContentAdded = false, promptPending = false;
    struct ToolKeyHash {
        std::size_t operator()(const AgentString& key) const noexcept {
            return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
        }
    };
    struct ToolKeyEqual {
        bool operator()(const AgentString& left, const AgentString& right) const noexcept {
            return left == right;
        }
    };
    using ToolBlockIndex = nicxlive::core::common::ResourceKeyMap<AgentString,
        std::size_t, ToolKeyHash, ToolKeyEqual>;
    ToolBlockIndex toolBlockIndex;
    bool initInFlight = false;
    Time initStart{};
    bool autoStartDone = false;
    Time lastStartAttempt{};
    struct NativeWorker {
        std::thread thread;
        std::exception_ptr failure;
    };
    std::shared_ptr<NativeWorker> worker;
    std::atomic<bool> workerRunning = false;
    Strings cmdQueue;
    compat::Slice<ACPResult> resultQueue;
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> qMutex;
    std::shared_ptr<nicxlive::core::d_runtime::SourceCondition> qCond;
    compat::Slice<PermissionRequest> permQueue;
    bool permChanged = false;
    AgentString agentLogPath;
    bool hasPlan = false;
    PlanView planView;

    void clearState();
    AgentString safeDecode(compat::ConstSlice<std::uint8_t> bytes);
    void log(AgentString message);
    AgentString getAgentLogPath();
    void appendAgentLog(AgentString line);
    void clearAgentLog();
    void renderClippedLines(ConstStrings lines, float wrapWidth, std::uint32_t dataVer,
        LineHeightCache& cache, bool prefixFirstOnly = false, AgentString prefix = emptyString(),
        const ImVec4* optColor = nullptr, std::size_t dirtyFrom = SIZE_MAX, std::size_t startIdx = 0);
    float measureConversationLines(ConstStrings lines, float wrapWidth,
        bool prefixFirstOnly = false, AgentString prefix = emptyString());
    Strings expandLines(ConstStrings source);
    Strings splitLines(AgentString text);
    AgentString shrinkLine(AgentString text, std::size_t limit);
    AgentString shrinkLineChat(AgentString text);
    AgentString shrinkLineLog(AgentString text);
    AgentString deriveUpdateKind(AgentJson update);
    AgentString deriveToolId(AgentJson update);
    AgentString deriveRoleFromKind(AgentString kind, AgentString toolId);
    AgentString extractTextFromContent(AgentJson update);
    AgentString fallbackText(AgentJson update);
    AgentString jsonScalarText(AgentJson object, AgentString key);
    AgentString firstScalar(AgentJson object, Strings keys);
    void updateUsage(AgentJson update);
    void handleSessionUpdate(AgentJson update);
    AgentString jsonScalar(AgentJson value);
    TrimView trimBlankEdges(ConstStrings lines);
    AgentString shortToolTitle(AgentString title);
    RoleKind roleKind(LogBlock block);
    void renderYou(LogBlock& block, float width, ImVec4 color);
    void renderTool(LogBlock& block, float width, bool isLast);
    void renderAssistantThinking(LogBlock& block, float width);
    void renderAssistantFinal(LogBlock& block, float width);
    void renderError(LogBlock& block, float width, bool isLast);
    void renderOther(LogBlock& block, float width, bool isLast);
    AgentJson mergeJson(AgentJson destination, AgentJson source);
    ImU32 statusColor(AgentString status);
    bool renderJsonFiltered(AgentJson value, float width, Strings focusKeys);
    bool renderJsonTree(AgentJson value, AgentString label, float wrapWidth,
        AgentString path = emptyString(), std::size_t siblingIdx = 0, Strings focusKeys = {}, bool allowAllBelow = false);
    void ensureBlock(AgentString role, AgentString title = emptyString());
    void pushChatLine(AgentString role, AgentString line, AgentJson payload = {},
        AgentString title = emptyString(), bool payloadPresent = false);
    void appendToLastLine(AgentString role, AgentString fragment, AgentJson payload = {},
        bool payloadPresent = false);
    void appendStream(AgentString role, AgentString text, AgentJson payload = {},
        AgentString title = emptyString(), bool payloadPresent = false, bool forceNewLine = false);
    LogBlock* findToolBlock(AgentString role);
    void handleToolUpdate(AgentString role, AgentString title, AgentString text,
        AgentJson payload, bool isInitial, AgentString status, AgentString toolId);
    void showPermissionToolCall(AgentJson params, AgentString permissionId, AgentString reason);
    void handlePlanUpdate(AgentString text, AgentJson payload, AgentString status);
    void flushPending();
    void enqueueUserText();
    void enqueueCommand(AgentString command);
    void drainResults();
    void pushResult(AgentString kind, bool isError, AgentString message);
    void signalWake();
    AgentString permissionOptionId(AgentJson option);
    AgentString permissionOptionLabel(AgentJson option);
    AgentString ellipsizePermissionOptionLabel(AgentString label);
    void showPermissionOptionTooltip(AgentString label, AgentString displayLabel);
    bool permissionOptionButton(AgentString label, AgentString optionId);
    void permissionOptionDisabledText(AgentString label);
    Jsons permissionOptions(AgentJson params);
    void renderPermissionRequest();
    void handleInbound(AgentJson object);
    void workerLoop(Strings command, AgentString workingDirectory);
    void stopWorker();
    void startAgent();
    void pingAgent();
    void stopAgent();
    Strings parseCommandLine(AgentString command);
    friend void ngAcpStopAll();
};

} // namespace nijigenerate::panels::agent

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::panels::agent::AgentPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.agent.AgentPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.agent.AgentPanel)";
};
} // namespace nicxlive::core::d_runtime
