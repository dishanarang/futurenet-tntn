#ifndef TNTN_WEIGHTED_SCORE_POLICY_H
#define TNTN_WEIGHTED_SCORE_POLICY_H

#include "tntn-access-selection-policy.h"

#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Select the available access with the highest weighted score of delay,
 * loss, rate and load, with hysteresis (M02-FR-09).
 *
 * Each metric is first normalized to a mark in [0,1], 1 being best, relative
 * to the other available accesses:
 *  - delay mark = lowest delay / this delay
 *  - loss mark  = 1 - lossRate
 *  - rate mark  = this rate / highest rate
 *  - load mark  = 1 - load
 *
 * score = (wd * delay + wl * loss + wr * rate + wo * load) / (wd + wl + wr + wo)
 *
 * Dividing by the weight sum normalizes the weights, so they need not sum
 * to 1; the score stays in [0,1].
 *
 * While the current access is available, the policy only moves to another
 * access if its score is at least HysteresisFraction higher:
 * candidateScore >= currentScore * (1 + HysteresisFraction).
 * If the current access is unavailable, the best access is chosen at once.
 * Ties go to the access listed first.
 */
class TntnWeightedScorePolicy : public TntnAccessSelectionPolicy
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnWeightedScorePolicy();
    ~TntnWeightedScorePolicy() override;

    /**
     * Compute the score of each access. Marks are relative to the accesses
     * passed in, so pass only the accesses being compared.
     * @param accesses accesses to score; must not be empty
     * @return one score in [0,1] per access, in the same order
     */
    std::vector<double> ComputeScores(const std::vector<TntnAccessState>& accesses) const;

  protected:
    TntnAccessDecision DoSelect(const std::vector<TntnAccessState>& available,
                                uint32_t currentAccessId) const override;

  private:
    double m_delayWeight; //!< Weight of the delay mark, >= 0
    double m_lossWeight;  //!< Weight of the loss mark, >= 0
    double m_rateWeight;  //!< Weight of the rate mark, >= 0
    double m_loadWeight;  //!< Weight of the load mark, >= 0
    double m_hysteresis;  //!< Required relative score improvement before switching, [0,0.5]
};

} // namespace ns3

#endif // TNTN_WEIGHTED_SCORE_POLICY_H
