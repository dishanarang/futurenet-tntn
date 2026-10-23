#include "tntn-helper.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/trace-helper.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnHelper");

namespace
{
/**
 * Format an access id for CSV; "no access" becomes an empty field.
 * @param accessId access id
 * @return the CSV field
 */
std::string
AccessToCsv(uint32_t accessId)
{
    return accessId == TNTN_NO_ACCESS ? "" : std::to_string(accessId);
}

/**
 * Find the first access of a type.
 * @param states access states
 * @param type access type
 * @return the state, or nullptr if no access has this type
 */
const TntnAccessState*
FirstOfType(const std::vector<TntnAccessState>& states, TntnAccessType type)
{
    for (const auto& s : states)
    {
        if (s.type == type)
        {
            return &s;
        }
    }
    return nullptr;
}

/**
 * Write one row of <prefix>-decisions.csv.
 * @param stream output file
 * @param manager manager that made the decision
 * @param current access in use at the decision
 * @param selected access chosen by the policy
 * @param reason policy reason
 */
void
WriteDecisionRow(Ptr<OutputStreamWrapper> stream,
                 const TntnAccessManager* manager,
                 uint32_t current,
                 uint32_t selected,
                 TntnReason reason)
{
    const auto& states = manager->GetLastAccessStates();
    const TntnAccessState* ter = FirstOfType(states, TntnAccessType::TERRESTRIAL);
    const TntnAccessState* ntn = FirstOfType(states, TntnAccessType::NTN);

    std::ostream& os = *stream->GetStream();
    os << Simulator::Now().GetSeconds() << ',' << manager->GetNodeId() << ','
       << AccessToCsv(current) << ',' << AccessToCsv(selected) << ',' << TntnReasonToString(reason)
       << ',';
    if (ter)
    {
        os << ter->delay.ToDouble(Time::MS);
    }
    os << ',';
    if (ntn)
    {
        os << ntn->delay.ToDouble(Time::MS);
    }
    os << ',';
    if (ter)
    {
        os << ter->lossRate;
    }
    os << ',';
    if (ntn)
    {
        os << ntn->lossRate;
    }
    // A handover started by this evaluation is still in progress here, because
    // its completion is scheduled after this row.
    os << ',' << (manager->IsHandoverInProgress() ? 1 : 0) << '\n';
}

/**
 * DecisionEvaluated sink.
 * @param stream output file
 * @param manager manager that fired the trace
 * @param nodeId node id
 * @param current access in use
 * @param selected access chosen by the policy
 * @param reason policy reason
 */
void
OnDecision(Ptr<OutputStreamWrapper> stream,
           const TntnAccessManager* manager,
           uint32_t nodeId,
           uint32_t current,
           uint32_t selected,
           TntnReason reason)
{
    // HandoverStart, if any, fires after this trace within the same evaluation,
    // so the row is written once the evaluation has finished.
    Simulator::ScheduleNow(&WriteDecisionRow, stream, manager, current, selected, reason);
}

/**
 * HandoverStart sink.
 * @param stream output file
 * @param nodeId node id
 * @param oldId old access
 * @param newId new access
 * @param reason reason
 */
void
OnHandoverStart(Ptr<OutputStreamWrapper> stream,
                uint32_t nodeId,
                uint32_t oldId,
                uint32_t newId,
                TntnReason reason)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ",tntn,HandoverStart," << nodeId
                         << ",,," << AccessToCsv(oldId) << ',' << AccessToCsv(newId)
                         << ",access_id," << TntnReasonToString(reason) << '\n';
}

/**
 * HandoverComplete sink.
 * @param stream output file
 * @param nodeId node id
 * @param oldId old access
 * @param newId new access
 */
void
OnHandoverComplete(Ptr<OutputStreamWrapper> stream, uint32_t nodeId, uint32_t oldId, uint32_t newId)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ",tntn,HandoverComplete," << nodeId
                         << ",,," << AccessToCsv(oldId) << ',' << AccessToCsv(newId)
                         << ",access_id,\n";
}

/**
 * AccessUnavailable sink.
 * @param stream output file
 * @param nodeId node id
 * @param accessId access that became unavailable
 */
void
OnAccessUnavailable(Ptr<OutputStreamWrapper> stream, uint32_t nodeId, uint32_t accessId)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ",tntn,AccessUnavailable," << nodeId
                         << ",,," << accessId << ",,access_id,\n";
}
} // namespace

TntnHelper::TntnHelper()
{
    m_managerFactory.SetTypeId(TntnAccessManager::GetTypeId());
    m_policyFactory.SetTypeId("ns3::TntnTerrestrialPreferredPolicy");
}

void
TntnHelper::SetManagerAttribute(const std::string& name, const AttributeValue& value)
{
    m_managerFactory.Set(name, value);
}

std::vector<Ptr<TntnAccessManager>>
TntnHelper::Install(NodeContainer nodes)
{
    std::vector<Ptr<TntnAccessManager>> managers;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        managers.push_back(Install(*it));
    }
    return managers;
}

Ptr<TntnAccessManager>
TntnHelper::Install(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    NS_ABORT_MSG_IF(m_accessFactories.empty(), "TntnHelper: call AddAccess before Install");
    NS_ABORT_MSG_IF(node->GetObject<TntnAccessManager>(),
                    "TntnHelper: node " << node->GetId() << " already has a manager");

    Ptr<TntnAccessManager> manager = m_managerFactory.Create<TntnAccessManager>();
    manager->SetNodeId(node->GetId());

    Ptr<TntnAccessSelectionPolicy> policy = m_policyFactory.Create<TntnAccessSelectionPolicy>();
    NS_ABORT_MSG_IF(!policy,
                    "TntnHelper: " << m_policyFactory.GetTypeId().GetName()
                                   << " is not a TntnAccessSelectionPolicy");
    manager->SetPolicy(policy);

    for (const auto& [accessId, factory] : m_accessFactories)
    {
        Ptr<TntnAccessStateProvider> provider = factory.Create<TntnAccessStateProvider>();
        NS_ABORT_MSG_IF(!provider,
                        "TntnHelper: " << factory.GetTypeId().GetName()
                                       << " is not a TntnAccessStateProvider");
        manager->AddAccess(accessId, provider);
    }

    node->AggregateObject(manager);
    manager->Start();
    m_managers.push_back(manager);
    return manager;
}

void
TntnHelper::EnableTracing(const std::string& prefix)
{
    NS_LOG_FUNCTION(this << prefix);
    NS_ABORT_MSG_IF(m_managers.empty(), "TntnHelper: call Install before EnableTracing");

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> decisions = ascii.CreateFileStream(prefix + "-decisions.csv");
    *decisions->GetStream() << "time_s,node_id,current_access,selected_access,reason,"
                               "ter_delay_ms,ntn_delay_ms,ter_loss,ntn_loss,handover\n";
    Ptr<OutputStreamWrapper> events = ascii.CreateFileStream(prefix + "-events.csv");
    *events->GetStream()
        << "time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail\n";

    for (const auto& manager : m_managers)
    {
        // A raw pointer avoids a reference cycle: the manager owns the trace
        // that holds this callback.
        manager->TraceConnectWithoutContext(
            "DecisionEvaluated",
            MakeBoundCallback(&OnDecision, decisions, PeekPointer(manager)));
        manager->TraceConnectWithoutContext("HandoverStart",
                                            MakeBoundCallback(&OnHandoverStart, events));
        manager->TraceConnectWithoutContext("HandoverComplete",
                                            MakeBoundCallback(&OnHandoverComplete, events));
        manager->TraceConnectWithoutContext("AccessUnavailable",
                                            MakeBoundCallback(&OnAccessUnavailable, events));
    }
}

} // namespace ns3
