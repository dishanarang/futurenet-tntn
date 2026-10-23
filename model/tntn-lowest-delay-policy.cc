#include "tntn-lowest-delay-policy.h"

#include "ns3/double.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnLowestDelayPolicy");
NS_OBJECT_ENSURE_REGISTERED(TntnLowestDelayPolicy);

TypeId
TntnLowestDelayPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnLowestDelayPolicy")
            .SetParent<TntnAccessSelectionPolicy>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<TntnLowestDelayPolicy>()
            .AddAttribute("HysteresisFraction",
                          "Fraction by which another access's delay must be lower than the "
                          "current access's delay before switching (dimensionless, [0,0.5]; "
                          "0.05 = at least 5% lower).",
                          DoubleValue(0.05),
                          MakeDoubleAccessor(&TntnLowestDelayPolicy::m_hysteresis),
                          MakeDoubleChecker<double>(0.0, 0.5));
    return tid;
}

TntnLowestDelayPolicy::TntnLowestDelayPolicy()
{
    NS_LOG_FUNCTION(this);
}

TntnLowestDelayPolicy::~TntnLowestDelayPolicy()
{
    NS_LOG_FUNCTION(this);
}

TntnAccessDecision
TntnLowestDelayPolicy::DoSelect(const std::vector<TntnAccessState>& available,
                                uint32_t currentAccessId) const
{
    NS_LOG_FUNCTION(this << available.size() << currentAccessId);

    const TntnAccessState* best = nullptr;
    const TntnAccessState* current = nullptr;
    for (const auto& s : available)
    {
        // Strict '<' keeps the first in list order on a tie, so results are deterministic.
        if (best == nullptr || s.delay < best->delay)
        {
            best = &s;
        }
        if (s.accessId == currentAccessId)
        {
            current = &s;
        }
    }

    TntnAccessDecision decision;
    decision.selectedAccessId = best->accessId;

    if (currentAccessId == TNTN_NO_ACCESS)
    {
        decision.reason = TntnReason::INITIAL_SELECTION;
    }
    else if (current == nullptr)
    {
        decision.reason = TntnReason::CURRENT_UNAVAILABLE;
    }
    else if (best == current)
    {
        decision.reason = TntnReason::NONE;
    }
    else
    {
        // Compared in seconds because the threshold is a fraction of the current delay.
        double threshold = current->delay.GetSeconds() * (1.0 - m_hysteresis);
        if (best->delay.GetSeconds() <= threshold)
        {
            decision.reason = TntnReason::BETTER_CANDIDATE;
        }
        else
        {
            NS_LOG_LOGIC("Access " << best->accessId << " is faster but within hysteresis");
            decision.selectedAccessId = current->accessId;
            decision.reason = TntnReason::NONE;
        }
    }

    NS_LOG_LOGIC("Selected access " << decision.selectedAccessId << " reason "
                                    << TntnReasonToString(decision.reason));
    return decision;
}

} // namespace ns3
