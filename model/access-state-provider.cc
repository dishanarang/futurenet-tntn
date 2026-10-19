#include "access-state-provider.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AccessStateProvider");
NS_OBJECT_ENSURE_REGISTERED(AccessStateProvider);

TypeId
AccessStateProvider::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AccessStateProvider")
                            .SetParent<Object>()
                            .SetGroupName("FuturenetTntn");
    return tid;
}

AccessStateProvider::~AccessStateProvider() = default;

} // namespace ns3