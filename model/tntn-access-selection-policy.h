#ifndef TNTN_ACCESS_SELECTION_POLICY_H
#define TNTN_ACCESS_SELECTION_POLICY_H

#include "tntn-access-types.h"

#include "ns3/object.h"

#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Base class for access selection policies.
 *
 * Select() removes unavailable accesses and then delegates the choice to
 * DoSelect(), so no subclass can ever pick an unavailable access (SRS T01).
 */
class TntnAccessSelectionPolicy : public Object
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    ~TntnAccessSelectionPolicy() override;

    /**
     * Choose an access among the candidates.
     *
     * @param candidates state of every access of the node
     * @param currentAccessId access in use now, or TNTN_NO_ACCESS if none
     * @return the decision; selectedAccessId is TNTN_NO_ACCESS if no
     *         candidate is available
     */
    TntnAccessDecision Select(const std::vector<TntnAccessState>& candidates,
                              uint32_t currentAccessId) const;

  protected:
    /**
     * Policy-specific choice, called by Select().
     *
     * @param available candidates that are available; never empty
     * @param currentAccessId access in use now, or TNTN_NO_ACCESS if none
     * @return the decision; must select one of @p available
     */
    virtual TntnAccessDecision DoSelect(const std::vector<TntnAccessState>& available,
                                        uint32_t currentAccessId) const = 0;
};

} // namespace ns3

#endif // TNTN_ACCESS_SELECTION_POLICY_H
