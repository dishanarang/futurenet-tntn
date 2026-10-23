#ifndef TNTN_TERRESTRIAL_PREFERRED_POLICY_H
#define TNTN_TERRESTRIAL_PREFERRED_POLICY_H

#include "tntn-access-selection-policy.h"

#include "ns3/data-rate.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Baseline policy: use terrestrial when it is good enough, otherwise NTN
 * (M02-FR-04).
 *
 * A terrestrial access is "good enough" when its delay, loss and rate all
 * meet the configured constraints. Rules, in order:
 *  1. If an available terrestrial access meets the constraints, select it.
 *  2. Otherwise select an available NTN access.
 *  3. If no NTN access is available, keep the best terrestrial access even
 *     though it violates the constraints, since a degraded access is better
 *     than none.
 * Within each rule the current access is kept if it qualifies, so the policy
 * never switches between two equally acceptable accesses; otherwise the one
 * with the lowest delay is chosen.
 */
class TntnTerrestrialPreferredPolicy : public TntnAccessSelectionPolicy
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnTerrestrialPreferredPolicy();
    ~TntnTerrestrialPreferredPolicy() override;

    /**
     * Check a state against the delay, loss and rate constraints.
     * @param state the access state
     * @return true if all constraints are met
     */
    bool MeetsConstraints(const TntnAccessState& state) const;

  protected:
    TntnAccessDecision DoSelect(const std::vector<TntnAccessState>& available,
                                uint32_t currentAccessId) const override;

  private:
    /**
     * Pick the best access of one type.
     * @param available available accesses
     * @param type access type to consider
     * @param requireConstraints if true, ignore accesses failing MeetsConstraints()
     * @param currentAccessId access in use now
     * @return the chosen access, or nullptr if none qualifies
     */
    const TntnAccessState* BestOfType(const std::vector<TntnAccessState>& available,
                                      TntnAccessType type,
                                      bool requireConstraints,
                                      uint32_t currentAccessId) const;

    /**
     * Explain why @p chosen was selected instead of the current access.
     * @param current state of the current access, or nullptr if it is not available
     * @param chosen the selected access
     * @param currentAccessId access in use now
     * @return the reason
     */
    TntnReason GetReason(const TntnAccessState* current,
                         const TntnAccessState& chosen,
                         uint32_t currentAccessId) const;

    /**
     * Set the minimum acceptable rate, rejecting values above the project range.
     * @param rate the minimum rate
     */
    void SetMinAcceptableRate(DataRate rate);
    /**
     * Get the minimum acceptable rate.
     * @return the minimum rate
     */
    DataRate GetMinAcceptableRate() const;

    Time m_maxDelay;    //!< Maximum acceptable one-way delay
    double m_maxLoss;   //!< Maximum acceptable loss ratio, [0,1]
    DataRate m_minRate; //!< Minimum acceptable data rate
};

} // namespace ns3

#endif // TNTN_TERRESTRIAL_PREFERRED_POLICY_H
