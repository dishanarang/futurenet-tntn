#include "tntn-access-manager.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnAccessManager");
NS_OBJECT_ENSURE_REGISTERED(TntnAccessManager);

TypeId
TntnAccessManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnAccessManager")
            .SetParent<Object>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<TntnAccessManager>()
            .AddAttribute("DecisionInterval",
                          "Time between periodic access evaluations (time, 100 ms - 10 s).",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&TntnAccessManager::m_decisionInterval),
                          MakeTimeChecker(MilliSeconds(100), Seconds(10)))
            .AddAttribute("MinDwellTime",
                          "Minimum time after a handover before an optional switch is allowed; "
                          "switches away from an unavailable access are never delayed "
                          "(time, 0 - 60 s).",
                          TimeValue(Seconds(5)),
                          MakeTimeAccessor(&TntnAccessManager::m_minDwellTime),
                          MakeTimeChecker(Seconds(0), Seconds(60)))
            .AddAttribute("HandoverExecutionDelay",
                          "Time from handover start to handover completion (time, 0 - 2 s).",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&TntnAccessManager::m_handoverDelay),
                          MakeTimeChecker(Seconds(0), Seconds(2)))
            .AddAttribute("MakeBeforeBreak",
                          "Keep using the old access until the new one is ready, when the old "
                          "access is still available at handover start (M02-FR-10). If false, "
                          "the node is detached during HandoverExecutionDelay.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TntnAccessManager::m_makeBeforeBreak),
                          MakeBooleanChecker())
            .AddTraceSource("DecisionEvaluated",
                            "A periodic or forced evaluation produced a decision.",
                            MakeTraceSourceAccessor(&TntnAccessManager::m_decisionEvaluatedTrace),
                            "ns3::TntnAccessManager::DecisionEvaluatedTracedCallback")
            .AddTraceSource("HandoverStart",
                            "A logical handover started.",
                            MakeTraceSourceAccessor(&TntnAccessManager::m_handoverStartTrace),
                            "ns3::TntnAccessManager::HandoverStartTracedCallback")
            .AddTraceSource("HandoverComplete",
                            "A logical handover completed; the new access is now current.",
                            MakeTraceSourceAccessor(&TntnAccessManager::m_handoverCompleteTrace),
                            "ns3::TntnAccessManager::HandoverCompleteTracedCallback")
            .AddTraceSource("AccessUnavailable",
                            "An access went from available to unavailable.",
                            MakeTraceSourceAccessor(&TntnAccessManager::m_accessUnavailableTrace),
                            "ns3::TntnAccessManager::AccessUnavailableTracedCallback");
    return tid;
}

TntnAccessManager::TntnAccessManager()
{
    NS_LOG_FUNCTION(this);
}

TntnAccessManager::~TntnAccessManager()
{
    NS_LOG_FUNCTION(this);
}

void
TntnAccessManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_evaluateEvent.Cancel();
    m_completeEvent.Cancel();
    m_accesses.clear();
    m_policy = nullptr;
    Object::DoDispose();
}

void
TntnAccessManager::SetNodeId(uint32_t nodeId)
{
    m_nodeId = nodeId;
}

uint32_t
TntnAccessManager::GetNodeId() const
{
    return m_nodeId;
}

void
TntnAccessManager::AddAccess(uint32_t accessId, Ptr<TntnAccessStateProvider> provider)
{
    NS_LOG_FUNCTION(this << accessId << provider);
    NS_ABORT_MSG_IF(!provider, "AddAccess: provider must not be null");
    NS_ABORT_MSG_IF(accessId == TNTN_NO_ACCESS, "AddAccess: TNTN_NO_ACCESS is reserved");
    NS_ABORT_MSG_IF(GetAccessProvider(accessId),
                    "AddAccess: access " << accessId << " already added");
    m_accesses.emplace_back(accessId, provider);
    // Assume available so an access that starts out down is reported once.
    m_lastAvailable.push_back(true);
}

void
TntnAccessManager::SetPolicy(Ptr<TntnAccessSelectionPolicy> policy)
{
    NS_LOG_FUNCTION(this << policy);
    NS_ABORT_MSG_IF(!policy, "SetPolicy: policy must not be null");
    m_policy = policy;
}

Ptr<TntnAccessSelectionPolicy>
TntnAccessManager::GetPolicy() const
{
    return m_policy;
}

void
TntnAccessManager::Start()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(m_evaluateEvent.IsPending(), "Start: manager already running");
    NS_ABORT_MSG_IF(!m_policy, "Start: no policy set");
    NS_ABORT_MSG_IF(m_accesses.empty(), "Start: no access added");
    m_evaluateEvent = Simulator::ScheduleNow(&TntnAccessManager::PeriodicEvaluate, this);
}

void
TntnAccessManager::Stop()
{
    NS_LOG_FUNCTION(this);
    m_evaluateEvent.Cancel();
}

void
TntnAccessManager::PeriodicEvaluate()
{
    Evaluate();
    m_evaluateEvent =
        Simulator::Schedule(m_decisionInterval, &TntnAccessManager::PeriodicEvaluate, this);
}

void
TntnAccessManager::ForceEvaluation()
{
    NS_LOG_FUNCTION(this);
    if (m_evaluateEvent.IsPending())
    {
        m_evaluateEvent.Cancel();
        PeriodicEvaluate();
    }
    else
    {
        Evaluate();
    }
}

void
TntnAccessManager::Evaluate()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_policy, "Evaluate: no policy set");

    // A decision made now would be based on an access that is about to change.
    if (m_handoverInProgress)
    {
        NS_LOG_LOGIC("Node " << m_nodeId << ": handover in progress, evaluation skipped");
        return;
    }

    CollectStates();
    TntnAccessDecision decision = m_policy->Select(m_lastStates, m_currentAccess);
    m_decisionEvaluatedTrace(m_nodeId, m_currentAccess, decision.selectedAccessId, decision.reason);

    // TNTN_NO_ACCESS means nothing is usable; keep the current access so the
    // node returns to it as soon as it recovers.
    if (decision.selectedAccessId == TNTN_NO_ACCESS || decision.selectedAccessId == m_currentAccess)
    {
        return;
    }
    if (BlockedByDwell(decision.reason))
    {
        NS_LOG_LOGIC("Node " << m_nodeId << ": switch to " << decision.selectedAccessId
                             << " blocked by dwell time");
        return;
    }
    StartHandover(decision.selectedAccessId, decision.reason);
}

bool
TntnAccessManager::ForceAccess(uint32_t accessId)
{
    NS_LOG_FUNCTION(this << accessId);
    Ptr<TntnAccessStateProvider> provider = GetAccessProvider(accessId);
    NS_ABORT_MSG_IF(!provider, "ForceAccess: access " << accessId << " not added");

    if (m_handoverInProgress)
    {
        return false;
    }
    if (accessId == m_currentAccess)
    {
        return true;
    }
    if (!provider->GetAccessState(accessId).available)
    {
        return false;
    }
    StartHandover(accessId, TntnReason::FORCED);
    return true;
}

uint32_t
TntnAccessManager::GetCurrentAccess() const
{
    return m_currentAccess;
}

bool
TntnAccessManager::IsHandoverInProgress() const
{
    return m_handoverInProgress;
}

const std::vector<TntnAccessState>&
TntnAccessManager::GetLastAccessStates() const
{
    return m_lastStates;
}

void
TntnAccessManager::CollectStates()
{
    m_lastStates.clear();
    for (std::size_t i = 0; i < m_accesses.size(); ++i)
    {
        const auto& [accessId, provider] = m_accesses[i];
        TntnAccessState state = provider->GetAccessState(accessId);
        NS_ABORT_MSG_IF(state.accessId != accessId,
                        "Provider for access " << accessId << " returned id " << state.accessId);
        if (m_lastAvailable[i] && !state.available)
        {
            m_accessUnavailableTrace(m_nodeId, accessId);
        }
        m_lastAvailable[i] = state.available;
        m_lastStates.push_back(state);
    }
}

bool
TntnAccessManager::BlockedByDwell(TntnReason reason) const
{
    if (!m_hasSwitched)
    {
        return false;
    }
    switch (reason)
    {
    case TntnReason::INITIAL_SELECTION:
    case TntnReason::CURRENT_UNAVAILABLE:
    case TntnReason::FORCED:
        return false;
    default:
        return Simulator::Now() - m_lastHandoverTime < m_minDwellTime;
    }
}

void
TntnAccessManager::StartHandover(uint32_t newAccessId, TntnReason reason)
{
    NS_LOG_FUNCTION(this << newAccessId << TntnReasonToString(reason));
    m_handoverInProgress = true;
    m_targetAccess = newAccessId;
    m_previousAccess = m_currentAccess;
    // The old access can only be kept while it still works.
    bool keepOld =
        m_makeBeforeBreak && m_currentAccess != TNTN_NO_ACCESS && IsAvailableNow(m_currentAccess);
    if (!keepOld)
    {
        m_currentAccess = TNTN_NO_ACCESS;
    }
    // Fired after detaching, so a listener can read GetCurrentAccess() to
    // tell break-before-make (detached) from make-before-break (still attached).
    m_handoverStartTrace(m_nodeId, m_previousAccess, newAccessId, reason);
    m_completeEvent =
        Simulator::Schedule(m_handoverDelay, &TntnAccessManager::CompleteHandover, this);
}

void
TntnAccessManager::CompleteHandover()
{
    NS_LOG_FUNCTION(this);
    uint32_t oldAccess = m_previousAccess;
    m_currentAccess = m_targetAccess;
    m_targetAccess = TNTN_NO_ACCESS;
    m_previousAccess = TNTN_NO_ACCESS;
    m_handoverInProgress = false;
    m_hasSwitched = true;
    m_lastHandoverTime = Simulator::Now();
    NS_LOG_INFO("Node " << m_nodeId << ": now on access " << m_currentAccess);
    m_handoverCompleteTrace(m_nodeId, oldAccess, m_currentAccess);
}

bool
TntnAccessManager::IsAvailableNow(uint32_t accessId) const
{
    Ptr<TntnAccessStateProvider> provider = GetAccessProvider(accessId);
    return provider && provider->GetAccessState(accessId).available;
}

Ptr<TntnAccessStateProvider>
TntnAccessManager::GetAccessProvider(uint32_t accessId) const
{
    for (const auto& [id, provider] : m_accesses)
    {
        if (id == accessId)
        {
            return provider;
        }
    }
    return nullptr;
}

} // namespace ns3
