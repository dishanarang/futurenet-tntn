#ifndef TNTN_LOWEST_DELAY_POLICY_H
#define TNTN_LOWEST_DELAY_POLICY_H

#include "tntn-access-selection-policy.h"

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Select the available access with the lowest delay, with hysteresis
 * (M02-FR-05, M02-FR-06).
 *
 * While the current access is available, the policy only moves to another
 * access if that access's delay is at least HysteresisFraction lower, i.e.
 * candidateDelay <= currentDelay * (1 - HysteresisFraction). This stops a
 * node flip-flopping between two accesses whose delays are almost equal.
 * If the current access is unavailable, the lowest-delay access is chosen
 * immediately. Ties go to the access listed first.
 */
class TntnLowestDelayPolicy : public TntnAccessSelectionPolicy
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnLowestDelayPolicy();
    ~TntnLowestDelayPolicy() override;

  protected:
    TntnAccessDecision DoSelect(const std::vector<TntnAccessState>& available,
                                uint32_t currentAccessId) const override;

  private:
    double m_hysteresis; //!< Required relative delay improvement before switching, [0,0.5]
};

} // namespace ns3

#endif // TNTN_LOWEST_DELAY_POLICY_H
