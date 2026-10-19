#include "ns3/core-module.h"
#include "ns3/mock-access-state-provider.h"

#include <iostream>

using namespace ns3;

static void
PrintState(Ptr<MockAccessStateProvider> ter, Ptr<MockAccessStateProvider> ntn)
{
    AccessState t = ter->GetAccessState(0);
    AccessState n = ntn->GetAccessState(1);
    std::cout << "t=" << Simulator::Now().GetSeconds() << "s  "
              << "TER avail=" << t.available << " delay=" << t.delay.GetMilliSeconds() << "ms  |  "
              << "NTN avail=" << n.available << " delay=" << n.delay.GetMilliSeconds() << "ms"
              << std::endl;
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Fake ground network: fast
    Ptr<MockAccessStateProvider> ter = CreateObject<MockAccessStateProvider>();
    ter->SetAccessType(TntnAccessType::TERRESTRIAL);
    ter->SetDelay(MilliSeconds(30));

    // Fake satellite: slower, a bit lossier
    Ptr<MockAccessStateProvider> ntn = CreateObject<MockAccessStateProvider>();
    ntn->SetAccessType(TntnAccessType::NTN);
    ntn->SetDelay(MilliSeconds(540));
    ntn->SetLossRate(0.02);
    ntn->SetRate(DataRate("10Mbps"));

    // Script: ground goes down at 3 s, comes back at 6 s, gets slow at 8 s
    ter->ScheduleAvailable(Seconds(3), false);
    ter->ScheduleAvailable(Seconds(6), true);
    ter->ScheduleDelay(Seconds(8), MilliSeconds(200));

    // Print both every second
    for (int i = 0; i <= 10; i++)
    {
        Simulator::Schedule(Seconds(i), &PrintState, ter, ntn);
    }

    Simulator::Stop(Seconds(10.5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}