#include "tntn-mock-access-state-provider.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnMockAccessStateProvider");
NS_OBJECT_ENSURE_REGISTERED(TntnMockAccessStateProvider);

TypeId
TntnMockAccessStateProvider::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnMockAccessStateProvider")
            .SetParent<TntnAccessStateProvider>()
            .SetGroupName("FuturenetTntn")
            .AddConstructor<TntnMockAccessStateProvider>()
            .AddAttribute("AccessType",
                          "Network kind reported for this access.",
                          EnumValue(TntnAccessType::TERRESTRIAL),
                          MakeEnumAccessor<TntnAccessType>(&TntnMockAccessStateProvider::m_type),
                          MakeEnumChecker(TntnAccessType::TERRESTRIAL,
                                          "Terrestrial",
                                          TntnAccessType::NTN,
                                          "Ntn"))
            .AddAttribute("Available",
                          "Whether the access can currently carry traffic.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&TntnMockAccessStateProvider::m_available),
                          MakeBooleanChecker())
            .AddAttribute("Delay",
                          "One-way delay estimate (time, >= 0).",
                          TimeValue(MilliSeconds(30)),
                          MakeTimeAccessor(&TntnMockAccessStateProvider::m_delay),
                          MakeTimeChecker(Seconds(0)))
            .AddAttribute("LossRate",
                          "Packet loss ratio (dimensionless, [0,1]).",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&TntnMockAccessStateProvider::m_lossRate),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("Rate",
                          "Available/nominal data rate (bit/s).",
                          DataRateValue(DataRate("50Mbps")),
                          MakeDataRateAccessor(&TntnMockAccessStateProvider::m_rate),
                          MakeDataRateChecker())
            .AddAttribute("Load",
                          "Normalized load (dimensionless, [0,1]; 0 = idle, 1 = saturated).",
                          DoubleValue(0.2),
                          MakeDoubleAccessor(&TntnMockAccessStateProvider::m_load),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

TntnMockAccessStateProvider::TntnMockAccessStateProvider()
{
    NS_LOG_FUNCTION(this);
}

TntnMockAccessStateProvider::~TntnMockAccessStateProvider()
{
    NS_LOG_FUNCTION(this);
}

TntnAccessState
TntnMockAccessStateProvider::GetAccessState(uint32_t accessId) const
{
    TntnAccessState s;
    s.accessId = accessId;
    s.type = m_type;
    s.available = m_available;
    s.delay = m_delay;
    s.lossRate = m_lossRate;
    s.rate = m_rate;
    s.load = m_load;
    return s;
}

void
TntnMockAccessStateProvider::SetAccessType(TntnAccessType type)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(type));
    m_type = type;
}

void
TntnMockAccessStateProvider::SetAvailable(bool available)
{
    NS_LOG_FUNCTION(this << available);
    m_available = available;
}

void
TntnMockAccessStateProvider::SetDelay(Time delay)
{
    NS_LOG_FUNCTION(this << delay);
    NS_ABORT_MSG_IF(delay.IsStrictlyNegative(), "Delay must be >= 0, got " << delay);
    m_delay = delay;
}

void
TntnMockAccessStateProvider::SetLossRate(double lossRate)
{
    NS_LOG_FUNCTION(this << lossRate);
    NS_ABORT_MSG_IF(lossRate < 0.0 || lossRate > 1.0,
                    "LossRate must be in [0,1], got " << lossRate);
    m_lossRate = lossRate;
}

void
TntnMockAccessStateProvider::SetRate(DataRate rate)
{
    NS_LOG_FUNCTION(this << rate);
    m_rate = rate;
}

void
TntnMockAccessStateProvider::SetLoad(double load)
{
    NS_LOG_FUNCTION(this << load);
    NS_ABORT_MSG_IF(load < 0.0 || load > 1.0, "Load must be in [0,1], got " << load);
    m_load = load;
}

Time
TntnMockAccessStateProvider::DelayUntil(Time at)
{
    NS_ABORT_MSG_IF(at < Simulator::Now(),
                    "Cannot schedule a change in the past (at="
                        << at.As(Time::S) << ", now=" << Simulator::Now().As(Time::S) << ")");
    return at - Simulator::Now();
}

// Values are validated here as well as in the setter so a bad scenario script
// fails where it is written, not later when the event fires.
// Ptr<>(this) keeps the mock alive until its scheduled events have run.

void
TntnMockAccessStateProvider::ScheduleAvailable(Time at, bool available)
{
    NS_LOG_FUNCTION(this << at << available);
    Simulator::Schedule(DelayUntil(at),
                        &TntnMockAccessStateProvider::SetAvailable,
                        Ptr<TntnMockAccessStateProvider>(this),
                        available);
}

void
TntnMockAccessStateProvider::ScheduleDelay(Time at, Time delay)
{
    NS_LOG_FUNCTION(this << at << delay);
    NS_ABORT_MSG_IF(delay.IsStrictlyNegative(), "Delay must be >= 0, got " << delay);
    Simulator::Schedule(DelayUntil(at),
                        &TntnMockAccessStateProvider::SetDelay,
                        Ptr<TntnMockAccessStateProvider>(this),
                        delay);
}

void
TntnMockAccessStateProvider::ScheduleLossRate(Time at, double lossRate)
{
    NS_LOG_FUNCTION(this << at << lossRate);
    NS_ABORT_MSG_IF(lossRate < 0.0 || lossRate > 1.0,
                    "LossRate must be in [0,1], got " << lossRate);
    Simulator::Schedule(DelayUntil(at),
                        &TntnMockAccessStateProvider::SetLossRate,
                        Ptr<TntnMockAccessStateProvider>(this),
                        lossRate);
}

void
TntnMockAccessStateProvider::ScheduleRate(Time at, DataRate rate)
{
    NS_LOG_FUNCTION(this << at << rate);
    Simulator::Schedule(DelayUntil(at),
                        &TntnMockAccessStateProvider::SetRate,
                        Ptr<TntnMockAccessStateProvider>(this),
                        rate);
}

void
TntnMockAccessStateProvider::ScheduleLoad(Time at, double load)
{
    NS_LOG_FUNCTION(this << at << load);
    NS_ABORT_MSG_IF(load < 0.0 || load > 1.0, "Load must be in [0,1], got " << load);
    Simulator::Schedule(DelayUntil(at),
                        &TntnMockAccessStateProvider::SetLoad,
                        Ptr<TntnMockAccessStateProvider>(this),
                        load);
}

} // namespace ns3
