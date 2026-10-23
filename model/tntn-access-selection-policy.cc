#include "tntn-access-selection-policy.h"

#include "ns3/abort.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnAccessSelectionPolicy");
NS_OBJECT_ENSURE_REGISTERED(TntnAccessSelectionPolicy);

TypeId
TntnAccessSelectionPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnAccessSelectionPolicy").SetParent<Object>().SetGroupName("FuturenetTntn");
    return tid;
}

TntnAccessSelectionPolicy::~TntnAccessSelectionPolicy() = default;

TntnAccessDecision
TntnAccessSelectionPolicy::Select(const std::vector<TntnAccessState>& candidates,
                                  uint32_t currentAccessId) const
{
    NS_LOG_FUNCTION(this << candidates.size() << currentAccessId);

    // Filtering here rather than in each subclass guarantees T01 for every
    // policy, including ones written later by other teams.
    std::vector<TntnAccessState> available;
    for (const auto& candidate : candidates)
    {
        if (candidate.available)
        {
            available.push_back(candidate);
        }
    }

    if (available.empty())
    {
        NS_LOG_LOGIC("No available access");
        return TntnAccessDecision{};
    }

    TntnAccessDecision decision = DoSelect(available, currentAccessId);

    // Guard against a buggy subclass: the chosen id must be one we offered.
    bool offered =
        std::any_of(available.begin(), available.end(), [&decision](const TntnAccessState& s) {
            return s.accessId == decision.selectedAccessId;
        });
    NS_ABORT_MSG_IF(!offered,
                    "Policy " << GetInstanceTypeId().GetName() << " selected access "
                              << decision.selectedAccessId << " which is not available");
    return decision;
}

} // namespace ns3
