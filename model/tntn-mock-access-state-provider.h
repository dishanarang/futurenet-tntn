#ifndef TNTN_MOCK_ACCESS_STATE_PROVIDER_H
#define TNTN_MOCK_ACCESS_STATE_PROVIDER_H

#include "tntn-access-state-provider.h"

#include "ns3/data-rate.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Access state provider whose metrics are set directly or scripted over time.
 *
 * Lets M02 be tested without M01 or any real network: one instance models one
 * access, and scenarios are built by scheduling metric changes (e.g., the
 * terrestrial access becomes unavailable at t = 3 s).
 */
class TntnMockAccessStateProvider : public TntnAccessStateProvider
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnMockAccessStateProvider();
    ~TntnMockAccessStateProvider() override;

    TntnAccessState GetAccessState(uint32_t accessId) const override;

    /**
     * Set the network kind reported for this access.
     * @param type the access type
     */
    void SetAccessType(TntnAccessType type);
    /**
     * Set whether the access is usable.
     * @param available true if usable
     */
    void SetAvailable(bool available);
    /**
     * Set the one-way delay estimate.
     * @param delay the delay; must be non-negative
     */
    void SetDelay(Time delay);
    /**
     * Set the packet loss ratio.
     * @param lossRate the loss ratio, in [0,1]
     */
    void SetLossRate(double lossRate);
    /**
     * Set the available/nominal data rate.
     * @param rate the data rate
     */
    void SetRate(DataRate rate);
    /**
     * Set the normalized load.
     * @param load the load, in [0,1]
     */
    void SetLoad(double load);

    /**
     * Schedule an availability change.
     * @param at absolute simulation time of the change; must not be in the past
     * @param available new availability
     */
    void ScheduleAvailable(Time at, bool available);
    /**
     * Schedule a delay change.
     * @param at absolute simulation time of the change; must not be in the past
     * @param delay new delay; must be non-negative
     */
    void ScheduleDelay(Time at, Time delay);
    /**
     * Schedule a loss ratio change.
     * @param at absolute simulation time of the change; must not be in the past
     * @param lossRate new loss ratio, in [0,1]
     */
    void ScheduleLossRate(Time at, double lossRate);
    /**
     * Schedule a data rate change.
     * @param at absolute simulation time of the change; must not be in the past
     * @param rate new data rate
     */
    void ScheduleRate(Time at, DataRate rate);
    /**
     * Schedule a load change.
     * @param at absolute simulation time of the change; must not be in the past
     * @param load new load, in [0,1]
     */
    void ScheduleLoad(Time at, double load);

  private:
    /**
     * Convert an absolute time to a delay from now, rejecting past times.
     * @param at absolute simulation time
     * @return delay until @p at
     */
    static Time DelayUntil(Time at);

    TntnAccessType m_type; //!< Network kind
    bool m_available;      //!< Whether the access is usable
    Time m_delay;          //!< One-way delay estimate
    double m_lossRate;     //!< Packet loss ratio, [0,1]
    DataRate m_rate;       //!< Available/nominal data rate
    double m_load;         //!< Normalized load, [0,1]
};

} // namespace ns3

#endif // TNTN_MOCK_ACCESS_STATE_PROVIDER_H
