#include "tntn-terrestrial-preferred-policy.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnTerrestrialPreferredPolicy");
NS_OBJECT_ENSURE_REGISTERED(TntnTerrestrialPreferredPolicy);

namespace
{
/// Upper end of the project range for MinAcceptableRate (SRS section 5)
const DataRate MAX_MIN_ACCEPTABLE_RATE("10Gbps");

/**
 * Find an access by id.
 * @param states accesses to search
 * @param accessId id to look for
 * @return the access, or nullptr if not present
 */
const TntnAccessState*
FindAccess(const std::vector<TntnAccessState>& states, uint32_t accessId)
{
    for (const auto& s : states)
    {
        if (s.accessId == accessId)
        {
            return &s;
        }
    }
    return nullptr;
}
} // namespace

TypeId
TntnTerrestrialPreferredPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnTerrestrialPreferredPolicy")
            .SetParent<TntnAccessSelectionPolicy>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<TntnTerrestrialPreferredPolicy>()
            .AddAttribute("MaxAcceptableDelay",
                          "Largest one-way delay at which terrestrial is still acceptable "
                          "(time, 1 ms - 5 s).",
                          TimeValue(MilliSeconds(150)),
                          MakeTimeAccessor(&TntnTerrestrialPreferredPolicy::m_maxDelay),
                          MakeTimeChecker(MilliSeconds(1), Seconds(5)))
            .AddAttribute("MaxAcceptableLoss",
                          "Largest loss ratio at which terrestrial is still acceptable "
                          "(dimensionless, [0,1]).",
                          DoubleValue(0.10),
                          MakeDoubleAccessor(&TntnTerrestrialPreferredPolicy::m_maxLoss),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute(
                "MinAcceptableRate",
                "Smallest data rate at which terrestrial is still acceptable "
                "(bit/s, 0 - 10 Gbit/s).",
                DataRateValue(DataRate("1Mbps")),
                MakeDataRateAccessor(&TntnTerrestrialPreferredPolicy::SetMinAcceptableRate,
                                     &TntnTerrestrialPreferredPolicy::GetMinAcceptableRate),
                MakeDataRateChecker());
    return tid;
}

TntnTerrestrialPreferredPolicy::TntnTerrestrialPreferredPolicy()
{
    NS_LOG_FUNCTION(this);
}

TntnTerrestrialPreferredPolicy::~TntnTerrestrialPreferredPolicy()
{
    NS_LOG_FUNCTION(this);
}

void
TntnTerrestrialPreferredPolicy::SetMinAcceptableRate(DataRate rate)
{
    NS_LOG_FUNCTION(this << rate);
    // DataRateChecker has no range support, so the SRS upper bound is enforced here.
    NS_ABORT_MSG_IF(rate > MAX_MIN_ACCEPTABLE_RATE,
                    "MinAcceptableRate must be <= " << MAX_MIN_ACCEPTABLE_RATE << ", got " << rate);
    m_minRate = rate;
}

DataRate
TntnTerrestrialPreferredPolicy::GetMinAcceptableRate() const
{
    return m_minRate;
}

bool
TntnTerrestrialPreferredPolicy::MeetsConstraints(const TntnAccessState& state) const
{
    return state.delay <= m_maxDelay && state.lossRate <= m_maxLoss && state.rate >= m_minRate;
}

const TntnAccessState*
TntnTerrestrialPreferredPolicy::BestOfType(const std::vector<TntnAccessState>& available,
                                           TntnAccessType type,
                                           bool requireConstraints,
                                           uint32_t currentAccessId) const
{
    const TntnAccessState* best = nullptr;
    for (const auto& s : available)
    {
        if (s.type != type || (requireConstraints && !MeetsConstraints(s)))
        {
            continue;
        }
        // Staying on the current access avoids a pointless handover.
        if (s.accessId == currentAccessId)
        {
            return &s;
        }
        // Strict '<' keeps the first in list order on a tie, so results are deterministic.
        if (best == nullptr || s.delay < best->delay)
        {
            best = &s;
        }
    }
    return best;
}

TntnReason
TntnTerrestrialPreferredPolicy::GetReason(const TntnAccessState* current,
                                          const TntnAccessState& chosen,
                                          uint32_t currentAccessId) const
{
    if (currentAccessId == TNTN_NO_ACCESS)
    {
        return TntnReason::INITIAL_SELECTION;
    }
    if (chosen.accessId == currentAccessId)
    {
        return TntnReason::NONE;
    }
    if (current == nullptr)
    {
        return TntnReason::CURRENT_UNAVAILABLE;
    }
    if (current->type == TntnAccessType::NTN && chosen.type == TntnAccessType::TERRESTRIAL)
    {
        return TntnReason::PREFERRED_RESTORED;
    }
    if (current->delay > m_maxDelay)
    {
        return TntnReason::DELAY_EXCEEDED;
    }
    if (current->lossRate > m_maxLoss)
    {
        return TntnReason::LOSS_EXCEEDED;
    }
    if (current->rate < m_minRate)
    {
        return TntnReason::RATE_TOO_LOW;
    }
    return TntnReason::BETTER_CANDIDATE;
}

TntnAccessDecision
TntnTerrestrialPreferredPolicy::DoSelect(const std::vector<TntnAccessState>& available,
                                         uint32_t currentAccessId) const
{
    NS_LOG_FUNCTION(this << available.size() << currentAccessId);

    const TntnAccessState* chosen =
        BestOfType(available, TntnAccessType::TERRESTRIAL, true, currentAccessId);
    if (chosen == nullptr)
    {
        chosen = BestOfType(available, TntnAccessType::NTN, false, currentAccessId);
    }
    if (chosen == nullptr)
    {
        chosen = BestOfType(available, TntnAccessType::TERRESTRIAL, false, currentAccessId);
    }
    // Every access is either TERRESTRIAL or NTN and 'available' is non-empty,
    // so one of the three rules always finds something.
    NS_ABORT_MSG_IF(chosen == nullptr, "No access selected from a non-empty candidate list");

    TntnAccessDecision decision;
    decision.selectedAccessId = chosen->accessId;
    decision.reason = GetReason(FindAccess(available, currentAccessId), *chosen, currentAccessId);
    decision.score = MeetsConstraints(*chosen) ? 1.0 : 0.0;
    NS_LOG_LOGIC("Selected access " << decision.selectedAccessId << " reason "
                                    << TntnReasonToString(decision.reason));
    return decision;
}

} // namespace ns3
