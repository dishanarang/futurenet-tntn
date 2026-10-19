#ifndef ACCESS_STATE_PROVIDER_H
#define ACCESS_STATE_PROVIDER_H

#include "tntn-access-types.h"

#include "ns3/object.h"

namespace ns3
{

/**
 * Interface for anything that can report the state of an access.
 * The mock provider implements it now; M01's satellite adapter will later.
 */
class AccessStateProvider : public Object
{
  public:
    static TypeId GetTypeId();
    ~AccessStateProvider() override;

    // "= 0" means: no code here, every child class MUST write this
    virtual AccessState GetAccessState(uint32_t accessId) const = 0;
};

} // namespace ns3

#endif // ACCESS_STATE_PROVIDER_H