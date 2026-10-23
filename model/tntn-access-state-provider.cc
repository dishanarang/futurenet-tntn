#include "tntn-access-state-provider.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TntnAccessStateProvider");
NS_OBJECT_ENSURE_REGISTERED(TntnAccessStateProvider);

TypeId
TntnAccessStateProvider::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TntnAccessStateProvider").SetParent<Object>().SetGroupName("FuturenetTntn");
    return tid;
}

TntnAccessStateProvider::~TntnAccessStateProvider() = default;

} // namespace ns3
