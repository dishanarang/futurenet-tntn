#ifndef TNTN_ACCESS_STATE_PROVIDER_H
#define TNTN_ACCESS_STATE_PROVIDER_H

#include "tntn-access-types.h"

#include "ns3/object.h"

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Interface through which the access manager reads the state of one access.
 *
 * This is the integration point for other modules: an M01 satellite adapter
 * or an M07 multi-access adapter implements it, and M02 never needs to know
 * their internals (AC-02, AC-06). TntnMockAccessStateProvider implements it
 * for stand-alone testing.
 */
class TntnAccessStateProvider : public Object
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    ~TntnAccessStateProvider() override;

    /**
     * Report the current state of an access.
     *
     * @param accessId identifier the manager uses for this access; copied
     *        into the returned state so the provider need not store it
     * @return the access state at the current simulation time
     */
    virtual TntnAccessState GetAccessState(uint32_t accessId) const = 0;
};

} // namespace ns3

#endif // TNTN_ACCESS_STATE_PROVIDER_H
