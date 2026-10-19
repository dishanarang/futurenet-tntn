#include "mock-access-state-provider.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MockAccessStateProvider");
NS_OBJECT_ENSURE_REGISTERED(MockAccessStateProvider);

TypeId
MockAccessStateProvider::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MockAccessStateProvider")
            .SetParent<AccessStateProvider>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<MockAccessStateProvider>()
            .AddAttribute("Available",
                          "Whether the access is currently usable",
                          BooleanValue(true),
                          MakeBooleanAccessor(&MockAccessStateProvider::m_available),
                          MakeBooleanChecker())
            .AddAttribute("Delay",
                          "One-way delay estimate",
                          TimeValue(MilliSeconds(30)),
                          MakeTimeAccessor(&MockAccessStateProvider::m_delay),
                          MakeTimeChecker())
            .AddAttribute("LossRate",
                          "Packet loss fraction (0-1)",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&MockAccessStateProvider::m_lossRate),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("Rate",
                          "Available/nominal data rate",
                          DataRateValue(DataRate("50Mbps")),
                          MakeDataRateAccessor(&MockAccessStateProvider::m_rate),
                          MakeDataRateChecker())
            .AddAttribute("Load",
                          "Load fraction (0 = idle, 1 = full)",
                          DoubleValue(0.2),
                          MakeDoubleAccessor(&MockAccessStateProvider::m_load),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

MockAccessStateProvider::MockAccessStateProvider()
    : m_type(TntnAccessType::TERRESTRIAL)
{
    NS_LOG_FUNCTION(this);
}

MockAccessStateProvider::~MockAccessStateProvider()
{
    NS_LOG_FUNCTION(this);
}

AccessState
MockAccessStateProvider::GetAccessState(uint32_t accessId) const
{
    AccessState s;
    s.accessId = accessId;
    s.type = m_type;
    s.available = m_available;
    s.delay = m_delay;
    s.lossRate = m_lossRate;
    s.rate = m_rate;
    s.load = m_load;
    return s;
}

// ---------- immediate setters ----------

void
MockAccessStateProvider::SetAccessType(TntnAccessType type)
{
    m_type = type;
}

void
MockAccessStateProvider::SetAvailable(bool available)
{
    NS_LOG_FUNCTION(this << available);
    m_available = available;
}

void
MockAccessStateProvider::SetDelay(Time delay)
{
    NS_LOG_FUNCTION(this << delay);
    m_delay = delay;
}

void
MockAccessStateProvider::SetLossRate(double lossRate)
{
    NS_ASSERT_MSG(lossRate >= 0.0 && lossRate <= 1.0, "LossRate must be 0-1");
    m_lossRate = lossRate;
}

void
MockAccessStateProvider::SetRate(DataRate rate)
{
    m_rate = rate;
}

void
MockAccessStateProvider::SetLoad(double load)
{
    NS_ASSERT_MSG(load >= 0.0 && load <= 1.0, "Load must be 0-1");
    m_load = load;
}

// ---------- scheduled changes ----------
// "at" is an absolute time; Simulator::Schedule wants "how long from now"

void
MockAccessStateProvider::ScheduleAvailable(Time at, bool available)
{
    NS_ASSERT(at >= Simulator::Now());
    Simulator::Schedule(at - Simulator::Now(),
                        &MockAccessStateProvider::SetAvailable, this, available);
}

void
MockAccessStateProvider::ScheduleDelay(Time at, Time delay)
{
    NS_ASSERT(at >= Simulator::Now());
    Simulator::Schedule(at - Simulator::Now(),
                        &MockAccessStateProvider::SetDelay, this, delay);
}

void
MockAccessStateProvider::ScheduleLossRate(Time at, double lossRate)
{
    NS_ASSERT(at >= Simulator::Now());
    Simulator::Schedule(at - Simulator::Now(),
                        &MockAccessStateProvider::SetLossRate, this, lossRate);
}

void
MockAccessStateProvider::ScheduleRate(Time at, DataRate rate)
{
    NS_ASSERT(at >= Simulator::Now());
    Simulator::Schedule(at - Simulator::Now(),
                        &MockAccessStateProvider::SetRate, this, rate);
}

void
MockAccessStateProvider::ScheduleLoad(Time at, double load)
{
    NS_ASSERT(at >= Simulator::Now());
    Simulator::Schedule(at - Simulator::Now(),
                        &MockAccessStateProvider::SetLoad, this, load);
}

} // namespace ns3