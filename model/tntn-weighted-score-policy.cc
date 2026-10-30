#include "tntn-weighted-score-policy.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnWeightedScorePolicy");
NS_OBJECT_ENSURE_REGISTERED(TntnWeightedScorePolicy);

TypeId
TntnWeightedScorePolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnWeightedScorePolicy")
            .SetParent<TntnAccessSelectionPolicy>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<TntnWeightedScorePolicy>()
            .AddAttribute("DelayWeight",
                          "Weight of the delay mark (dimensionless, >= 0; weights are "
                          "normalized by their sum).",
                          DoubleValue(0.40),
                          MakeDoubleAccessor(&TntnWeightedScorePolicy::m_delayWeight),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("LossWeight",
                          "Weight of the loss mark (dimensionless, >= 0).",
                          DoubleValue(0.20),
                          MakeDoubleAccessor(&TntnWeightedScorePolicy::m_lossWeight),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("RateWeight",
                          "Weight of the rate mark (dimensionless, >= 0).",
                          DoubleValue(0.20),
                          MakeDoubleAccessor(&TntnWeightedScorePolicy::m_rateWeight),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("LoadWeight",
                          "Weight of the load mark (dimensionless, >= 0).",
                          DoubleValue(0.20),
                          MakeDoubleAccessor(&TntnWeightedScorePolicy::m_loadWeight),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("HysteresisFraction",
                          "Fraction by which another access's score must exceed the current "
                          "access's score before switching (dimensionless, [0,0.5]; "
                          "0.05 = at least 5% higher).",
                          DoubleValue(0.05),
                          MakeDoubleAccessor(&TntnWeightedScorePolicy::m_hysteresis),
                          MakeDoubleChecker<double>(0.0, 0.5));
    return tid;
}

TntnWeightedScorePolicy::TntnWeightedScorePolicy()
{
    NS_LOG_FUNCTION(this);
}

TntnWeightedScorePolicy::~TntnWeightedScorePolicy()
{
    NS_LOG_FUNCTION(this);
}

std::vector<double>
TntnWeightedScorePolicy::ComputeScores(const std::vector<TntnAccessState>& accesses) const
{
    NS_ABORT_MSG_IF(accesses.empty(), "ComputeScores: no access to score");
    double weightSum = m_delayWeight + m_lossWeight + m_rateWeight + m_loadWeight;
    NS_ABORT_MSG_IF(weightSum <= 0.0, "TntnWeightedScorePolicy: at least one weight must be > 0");

    // Delay and rate marks are relative to the best access being compared,
    // because they have no natural upper bound.
    double minDelayS = accesses.front().delay.GetSeconds();
    uint64_t maxRateBps = 0;
    for (const auto& s : accesses)
    {
        minDelayS = std::min(minDelayS, s.delay.GetSeconds());
        maxRateBps = std::max(maxRateBps, s.rate.GetBitRate());
    }

    std::vector<double> scores;
    scores.reserve(accesses.size());
    for (const auto& s : accesses)
    {
        double delayS = s.delay.GetSeconds();
        // A zero delay is the best possible; it also avoids dividing by zero.
        double delayMark = (delayS > 0.0) ? minDelayS / delayS : 1.0;
        double lossMark = 1.0 - s.lossRate;
        // If every rate is zero, rate cannot tell the accesses apart.
        double rateMark =
            (maxRateBps > 0) ? static_cast<double>(s.rate.GetBitRate()) / maxRateBps : 1.0;
        double loadMark = 1.0 - s.load;

        double score = (m_delayWeight * delayMark + m_lossWeight * lossMark +
                        m_rateWeight * rateMark + m_loadWeight * loadMark) /
                       weightSum;
        NS_LOG_LOGIC("Access " << s.accessId << ": delay " << delayMark << " loss " << lossMark
                               << " rate " << rateMark << " load " << loadMark << " -> score "
                               << score);
        scores.push_back(score);
    }
    return scores;
}

TntnAccessDecision
TntnWeightedScorePolicy::DoSelect(const std::vector<TntnAccessState>& available,
                                  uint32_t currentAccessId) const
{
    NS_LOG_FUNCTION(this << available.size() << currentAccessId);

    std::vector<double> scores = ComputeScores(available);
    std::size_t best = 0;
    std::size_t current = available.size(); // "not found"
    for (std::size_t i = 0; i < available.size(); ++i)
    {
        // Strict '>' keeps the first in list order on a tie, so results are deterministic.
        if (scores[i] > scores[best])
        {
            best = i;
        }
        if (available[i].accessId == currentAccessId)
        {
            current = i;
        }
    }

    TntnAccessDecision decision;
    decision.selectedAccessId = available[best].accessId;
    decision.score = scores[best];

    if (currentAccessId == TNTN_NO_ACCESS)
    {
        decision.reason = TntnReason::INITIAL_SELECTION;
    }
    else if (current == available.size())
    {
        decision.reason = TntnReason::CURRENT_UNAVAILABLE;
    }
    else if (best == current)
    {
        decision.reason = TntnReason::NONE;
    }
    else if (scores[best] >= scores[current] * (1.0 + m_hysteresis))
    {
        decision.reason = TntnReason::BETTER_CANDIDATE;
    }
    else
    {
        NS_LOG_LOGIC("Access " << available[best].accessId
                               << " scores higher but within hysteresis");
        decision.selectedAccessId = available[current].accessId;
        decision.score = scores[current];
        decision.reason = TntnReason::NONE;
    }

    NS_LOG_LOGIC("Selected access " << decision.selectedAccessId << " reason "
                                    << TntnReasonToString(decision.reason));
    return decision;
}

} // namespace ns3
