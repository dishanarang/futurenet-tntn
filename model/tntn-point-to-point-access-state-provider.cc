#include "tntn-point-to-point-access-state-provider.h"

#include "ns3/abort.h"
#include "ns3/enum.h"
#include "ns3/error-model.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/pointer.h"
#include "ns3/queue.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnPointToPointAccessStateProvider");
NS_OBJECT_ENSURE_REGISTERED(TntnPointToPointAccessStateProvider);

TypeId
TntnPointToPointAccessStateProvider::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TntnPointToPointAccessStateProvider")
                            .SetParent<TntnAccessStateProvider>()
                            .SetGroupName("FuturenetTntn")
                            .AddConstructor<TntnPointToPointAccessStateProvider>()
                            .AddAttribute("AccessType",
                                          "Network kind reported for this access.",
                                          EnumValue(TntnAccessType::TERRESTRIAL),
                                          MakeEnumAccessor<TntnAccessType>(
                                              &TntnPointToPointAccessStateProvider::m_type),
                                          MakeEnumChecker(TntnAccessType::TERRESTRIAL,
                                                          "Terrestrial",
                                                          TntnAccessType::NTN,
                                                          "Ntn"));
    return tid;
}

TntnPointToPointAccessStateProvider::TntnPointToPointAccessStateProvider()
{
    NS_LOG_FUNCTION(this);
}

TntnPointToPointAccessStateProvider::~TntnPointToPointAccessStateProvider()
{
    NS_LOG_FUNCTION(this);
}

void
TntnPointToPointAccessStateProvider::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_device = nullptr;
    TntnAccessStateProvider::DoDispose();
}

void
TntnPointToPointAccessStateProvider::SetDevice(Ptr<PointToPointNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    NS_ABORT_MSG_IF(!device, "SetDevice: device must not be null");
    m_device = device;
}

Ptr<PointToPointNetDevice>
TntnPointToPointAccessStateProvider::GetDevice() const
{
    return m_device;
}

TntnAccessState
TntnPointToPointAccessStateProvider::GetAccessState(uint32_t accessId) const
{
    NS_ABORT_MSG_IF(!m_device, "No device set; call SetDevice() before the manager starts");
    auto channel = DynamicCast<PointToPointChannel>(m_device->GetChannel());
    NS_ABORT_MSG_IF(!channel, "Device is not attached to a PointToPointChannel");

    TntnAccessState s;
    s.accessId = accessId;
    s.type = m_type;

    // A point-to-point link only goes down when it has no channel, so outages
    // are modelled by switching the IPv4 interface off (Ipv4::SetDown).
    s.available = m_device->IsLinkUp();
    Ptr<Ipv4> ipv4 = m_device->GetNode()->GetObject<Ipv4>();
    if (ipv4)
    {
        int32_t interface = ipv4->GetInterfaceForDevice(m_device);
        s.available = s.available && interface >= 0 && ipv4->IsUp(interface);
    }

    TimeValue delay;
    channel->GetAttribute("Delay", delay);
    s.delay = delay.Get();

    DataRateValue rate;
    m_device->GetAttribute("DataRate", rate);
    s.rate = rate.Get();

    // Packets sent on this access are dropped by the far end's receive error model.
    Ptr<PointToPointNetDevice> farEnd = channel->GetPointToPointDevice(0) == m_device
                                            ? channel->GetPointToPointDevice(1)
                                            : channel->GetPointToPointDevice(0);
    PointerValue errorModel;
    farEnd->GetAttribute("ReceiveErrorModel", errorModel);
    auto rateModel = errorModel.Get<RateErrorModel>();
    if (rateModel && rateModel->IsEnabled() &&
        rateModel->GetUnit() == RateErrorModel::ERROR_UNIT_PACKET)
    {
        s.lossRate = rateModel->GetRate();
    }

    Ptr<Queue<Packet>> queue = m_device->GetQueue();
    if (queue && queue->GetMaxSize().GetValue() > 0)
    {
        double fill = static_cast<double>(queue->GetCurrentSize().GetValue()) /
                      queue->GetMaxSize().GetValue();
        s.load = std::min(1.0, fill);
    }
    return s;
}

} // namespace ns3
