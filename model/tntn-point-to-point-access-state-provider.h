#ifndef TNTN_POINT_TO_POINT_ACCESS_STATE_PROVIDER_H
#define TNTN_POINT_TO_POINT_ACCESS_STATE_PROVIDER_H

#include "tntn-access-state-provider.h"

#include "ns3/point-to-point-net-device.h"

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Access state provider that reads a real ns-3 point-to-point link.
 *
 * Serves as the adapter between M02 and a standard ns-3 network model, and as
 * the template for other adapters (e.g. an M01 satellite provider). The
 * device given to SetDevice() is the node-side end of the access link.
 *
 * Mapping:
 *  - available: device link is up and its IPv4 interface (if any) is up
 *  - delay: the channel's "Delay" attribute (propagation delay)
 *  - rate: the device's "DataRate" attribute
 *  - lossRate: the packet error rate of a RateErrorModel installed as
 *    "ReceiveErrorModel" on the far-end device, else 0
 *  - load: occupancy of the device's transmit queue, in [0,1]
 */
class TntnPointToPointAccessStateProvider : public TntnAccessStateProvider
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnPointToPointAccessStateProvider();
    ~TntnPointToPointAccessStateProvider() override;

    /**
     * Set the node-side device of the access link.
     * @param device the device; must be attached to a PointToPointChannel
     */
    void SetDevice(Ptr<PointToPointNetDevice> device);
    /**
     * Get the node-side device of the access link.
     * @return the device
     */
    Ptr<PointToPointNetDevice> GetDevice() const;

    TntnAccessState GetAccessState(uint32_t accessId) const override;

  protected:
    void DoDispose() override;

  private:
    TntnAccessType m_type;               //!< Network kind reported for this access
    Ptr<PointToPointNetDevice> m_device; //!< Node-side device of the access link
};

} // namespace ns3

#endif // TNTN_POINT_TO_POINT_ACCESS_STATE_PROVIDER_H
