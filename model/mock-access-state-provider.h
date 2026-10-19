#ifndef MOCK_ACCESS_STATE_PROVIDER_H
#define MOCK_ACCESS_STATE_PROVIDER_H

#include "access-state-provider.h"

#include "ns3/data-rate.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * A fake access whose metrics are set by hand or scheduled over time.
 * One mock = one access (e.g., one for terrestrial, one for NTN).
 */
class MockAccessStateProvider : public AccessStateProvider
{
  public:
    static TypeId GetTypeId();
    MockAccessStateProvider();
    ~MockAccessStateProvider() override;

    AccessState GetAccessState(uint32_t accessId) const override;

    // Change values immediately
    void SetAccessType(TntnAccessType type);
    void SetAvailable(bool available);
    void SetDelay(Time delay);
    void SetLossRate(double lossRate);
    void SetRate(DataRate rate);
    void SetLoad(double load);

    // Change values at an absolute simulation time
    void ScheduleAvailable(Time at, bool available);
    void ScheduleDelay(Time at, Time delay);
    void ScheduleLossRate(Time at, double lossRate);
    void ScheduleRate(Time at, DataRate rate);
    void ScheduleLoad(Time at, double load);

  private:
    TntnAccessType m_type;
    bool m_available;
    Time m_delay;
    double m_lossRate;
    DataRate m_rate;
    double m_load;
};

} // namespace ns3

#endif // MOCK_ACCESS_STATE_PROVIDER_H